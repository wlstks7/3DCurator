﻿#include "woodSegmentation.h"

bool longerLine(HoughLine i, HoughLine j) {
	return i.second > j.second;
}

std::vector<Line> getLinesFromImage(Figura *figura, int slice) {
	vtkSmartPointer<vtkImageMapToColors> map = vtkSmartPointer<vtkImageMapToColors>::New();
	map->SetInputData(figura->getImageData());
	map->SetLookupTable(figura->getTransferFunction()->getColorFun());

	vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
	voi->SetInputConnection(map->GetOutputPort());
	voi->SetVOI(0, figura->getImageData()->GetDimensions()[0] - 1, 0, figura->getImageData()->GetDimensions()[1] - 1, slice, slice);
	voi->Update();

	vtkSmartPointer<vtkPNGWriter> png = vtkSmartPointer<vtkPNGWriter>::New();
	png->SetInputData(voi->GetOutput());
	png->SetFileName(("tmp" + std::to_string(slice) + ".png").c_str());
	png->Write();

	cv::Mat src, dst, cdst;
	src = cv::imread(("tmp" + std::to_string(slice) + ".png").c_str(), 0);

	cv::Canny(src, dst, 30, 50, 3);
	cv::cvtColor(dst, cdst, cv::COLOR_GRAY2BGR);

	std::vector<cv::Vec4i> lines;
	cv::HoughLinesP(dst, lines, 1, CV_PI / 180, 50, 50, 10);
	std::vector<HoughLine> foundLines;
	for (int i = 0; i < lines.size(); i++) {
		HoughLine l = std::make_pair(
			std::make_pair(
				cv::Point(lines[i][0], lines[i][1]),
				cv::Point(lines[i][2], lines[i][3])
			),
			cv::norm(cv::Point(lines[i][0], lines[i][1]) - cv::Point(lines[i][2], lines[i][3])));
		foundLines.push_back(l);
	}
	sort(foundLines.begin(), foundLines.end(), longerLine);

	remove(("tmp" + std::to_string(slice) + ".png").c_str());

	std::vector<Line> result(foundLines.size());

	for (int i = 0; i < foundLines.size(); i++) {
		result[i] = foundLines[i].first;
	}

	return result;
}

std::string generateImage(Figura *figura, int slice, std::vector<Line> lines) {
	std::string filename = "tmp" + std::to_string(slice) + ".png";

	vtkSmartPointer<vtkImageMapToColors> map = vtkSmartPointer<vtkImageMapToColors>::New();
	map->SetInputData(figura->getImageData());
	map->SetLookupTable(figura->getTransferFunction()->getColorFun());

	vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
	voi->SetInputConnection(map->GetOutputPort());
	voi->SetVOI(0, figura->getImageData()->GetDimensions()[0] - 1, 0, figura->getImageData()->GetDimensions()[1] - 1, slice, slice);
	voi->Update();

	vtkSmartPointer<vtkPNGWriter> png = vtkSmartPointer<vtkPNGWriter>::New();
	png->SetInputData(voi->GetOutput());
	png->SetFileName(filename.c_str());
	png->Write();

	cv::Mat src;
	src = cv::imread(filename.c_str(), 1);

	remove(filename.c_str());

	int numLines = lines.size() > 6 ? 6 : lines.size();

	for (int i = 0; i < numLines; i++) {
		cv::Point p0 = lines[i].first, p1 = lines[i].second;
		cv::Scalar color;
		switch (i) {
		case 0:
			color = cv::Scalar(0, 0, 255);
			break;
		case 1:
			color = cv::Scalar(0, 255, 0);
			break;
		case 2:
			color = cv::Scalar(255, 0, 0);
			break;
		case 3:
			color = cv::Scalar(0, 255, 255);
			break;
		case 4:
			color = cv::Scalar(255, 255, 0);
			break;
		case 5:
			color = cv::Scalar(255, 0, 255);
			break;
		case 6:
			color = cv::Scalar(255, 255, 255);
			break;
		default:
			color = cv::Scalar(128, 128, 128);
			break;
		}
		line(src, p0, p1, color, 1, cv::LINE_AA);
	}

	std::vector<int> compression_params;
	compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
	compression_params.push_back(9);
	imwrite(filename, src, compression_params);

	return filename;
}

bool isInLine(Coord2D coord, LineEq eq, int epsilon) {
	double result = coord[1] - epsilon + (eq[0] * coord[0] + eq[1]);
	if (result >= -LINE_TOLERANCE && result <= LINE_TOLERANCE) {
		return true;
	}
	else {
		return false;
	}
}

bool isAdjacent(Figura *figura, Coord3D coord, double MIN, double MAX) {
	double value = figura->getImageData()->GetScalarComponentAsFloat(coord[0], coord[1], coord[2], 0);
	if (value >= MIN && value <= MAX) {
		return true;
	}
	else {
		return false;
	}
}

Coord2D searchInitialVoxel(Figura *figura, const int ijk[3], Bounds bounds, double MIN, double MAX, LineEq eq) {
	Coord2D ij;
	std::queue<Coord2D> queue;

	ij[0] = ijk[0]; ij[1] = ijk[1];

	queue.push(ij);
	while (!queue.empty()) {
		ij = queue.front();
		queue.pop();
		if (ij[0] < bounds.MAX_X && ij[0] >= bounds.MIN_X && ij[1] < bounds.MAX_Y && ij[1] >= bounds.MIN_Y
			&& !isInLine(ij, eq, bounds.MAX_X)) {
			if (isAdjacent(figura, { ij[0], ij[1], ijk[2] }, MIN_WOOD, MAX_WOOD)) {
				return ij;
			}
			else {
				queue.push({ ij[0] - 1, ij[1] - 1 });
				queue.push({ ij[0] - 1, ij[1] });
				queue.push({ ij[0] - 1, ij[1] + 1 });
				queue.push({ ij[0], ij[1] - 1 });
				queue.push({ ij[0], ij[1] + 1 });
				queue.push({ ij[0] + 1, ij[1] - 1 });
				queue.push({ ij[0] + 1, ij[1] });
				queue.push({ ij[0] + 1, ij[1] + 1 });
			}
		}
	}

	return{ ijk[0], ijk[1] };
}
Coord2D regionGrowingWithLineBoundImage(Figura *figura, const int ijk[3], Bounds bounds, LineEq eq) {
	Coord2D ij;
	Coord2D min = { bounds.MAX_X, bounds.MAX_Y };
	Coord2D max = { bounds.MIN_X, bounds.MIN_Y };
	std::stack<Coord2D> stack;

	ij[0] = ijk[0]; ij[1] = ijk[1]; // voxel inicial

	stack.push(ij); // añade el primer voxel
	while (!stack.empty()) { // continua hasta vaciar la pila
		ij = stack.top(); // primer elemento de la pila
		stack.pop(); // elimina el primer elemento de la pila
		if (ij[0] < bounds.MAX_X && ij[0] >= bounds.MIN_X && ij[1] < bounds.MAX_Y && ij[1] >= bounds.MIN_Y) { // se encuentra entre los l�mites
			if (isAdjacent(figura, { ij[0], ij[1], ijk[2] }, MIN_WOOD, MAX_WOOD)
				&& !isInLine(ij, eq, bounds.MAX_X)) {
				figura->getImageData()->SetScalarComponentFromFloat(ij[0], ij[1], ijk[2], 0, AIR_HU); // actualiza el voxel con el valor del aire
																											 // añade a la pila los voxeles de alrededor
				stack.push({ ij[0] - 1, ij[1] - 1 });
				stack.push({ ij[0] - 1, ij[1] });
				stack.push({ ij[0] - 1, ij[1] + 1 });
				stack.push({ ij[0], ij[1] - 1 });
				stack.push({ ij[0], ij[1] + 1 });
				stack.push({ ij[0] + 1, ij[1] - 1 });
				stack.push({ ij[0] + 1, ij[1] });
				stack.push({ ij[0] + 1, ij[1] + 1 });
				// actualiza punto medio
				if (ij[0] < min[0]) {
					min[0] = ij[0];
				}
				if (ij[1] < min[1]) {
					min[1] = ij[1];
				}
				if (ij[0] > max[0]) {
					max[0] = ij[0];
				}
				if (ij[1] > max[1]) {
					max[1] = ij[1];
				}
			}
		}
	}

	return{ (min[0] + max[0]) / 2 , (min[1] + max[1]) / 2 };
}

std::pair<Line, double> findNearestLine(std::vector<Line> lines, Line goal, const int originalZ, const int z) {
	double minAngle = 90;
	double min = 0;
	int A[3] = { goal.first.x, goal.first.y, originalZ };
	int A_[3] = { goal.second.x, goal.second.y, originalZ };
	for (int i = 0; i < lines.size(); i++) {
		int B[3] = { lines[i].first.x, lines[i].first.y, z };
		int B_[3] = { lines[i].second.x, lines[i].second.y, z };
		double distance = getPointLineDistance(A, A_, B);
		double angle = getAngle(A, A_, B, B_);
		if (angle < minAngle && distance < MIN_DISTA) {
			min = i;
			minAngle = angle;
		}
	}
	return std::make_pair(lines[min], minAngle);
}

void regionGrowingWithLineBoundVolume(Figura *figura, const int ijk[3], Bounds bounds, Line firstLine, std::vector<std::vector<Line> > &lines) {
	int xyz[3] = { ijk[0], ijk[1], ijk[2] }; // voxel inicial
	Line lastLine = firstLine;
	int lastZ = ijk[2];
	int U[3] = { lastLine.first.x, lastLine.first.y, lastZ };
	int V[3] = { lastLine.second.x, lastLine.second.y, lastZ };
	Coord2D lastCentroid = regionGrowingWithLineBoundImage(figura, xyz, bounds, getLineEquation(U, V));
	Coord2D lastCentroidAux = lastCentroid;

	int numberOfNoLines = 0;

	// Borrar hacia arriba
	xyz[2] = ijk[2] + 1;
	while (xyz[2] < bounds.MAX_Z) {
		lines[xyz[2]] = getLinesFromImage(figura, xyz[2]);
		std::pair<Line, double> nearestLine = findNearestLine(lines[xyz[2]], lastLine, lastZ, xyz[2]);
		if (nearestLine.second < MIN_ANGLE) {
			if (numberOfNoLines != 0) {
				int A[3] = { lastLine.first.x, lastLine.first.y, lastZ };
				int B[3] = { lastLine.second.x, lastLine.second.y, lastZ };
				int C[3] = { nearestLine.first.first.x, nearestLine.first.first.y, xyz[2] };
				PlaneEq P = getPlaneEquation(A, B, C);
				int xyzAux[3] = { xyz[0], xyz[1], xyz[2] - numberOfNoLines };
				for (; xyzAux[2] < xyz[2]; xyzAux[2]++) {
					if (!isAdjacent(figura, { xyzAux[0], xyzAux[1], xyzAux[2] }, MIN_WOOD, MAX_WOOD)) {
						xyzAux[0] = lastCentroid[0];
						xyzAux[1] = lastCentroid[1];
					}
					if (!isAdjacent(figura, { xyzAux[0], xyzAux[1], xyzAux[2] }, MIN_WOOD, MAX_WOOD)) {
						Coord2D newCentroid = searchInitialVoxel(figura, xyzAux, bounds, MIN_WOOD, MAX_WOOD, getLineEquationFromPlane(P, xyzAux[2]));
						xyzAux[0] = newCentroid[0];
						xyzAux[1] = newCentroid[1];
					}
					lastCentroid = regionGrowingWithLineBoundImage(figura, xyzAux, bounds, getLineEquationFromPlane(P, xyzAux[2]));
				} // for
			} // if (numberOfNoLines != 0)
			lastLine = nearestLine.first;
			lastZ = xyz[2];
			int U[3] = { lastLine.first.x, lastLine.first.y, lastZ };
			int V[3] = { lastLine.second.x, lastLine.second.y, lastZ };
			if (!isAdjacent(figura, { xyz[0], xyz[1], xyz[2] }, MIN_WOOD, MAX_WOOD)) {
				xyz[0] = lastCentroid[0];
				xyz[1] = lastCentroid[1];
			}
			if (!isAdjacent(figura, { xyz[0], xyz[1], xyz[2] }, MIN_WOOD, MAX_WOOD)) {
				Coord2D newCentroid = searchInitialVoxel(figura, xyz, bounds, MIN_WOOD, MAX_WOOD, getLineEquation(U, V));
				xyz[0] = newCentroid[0];
				xyz[1] = newCentroid[1];
			}
			lastCentroid = regionGrowingWithLineBoundImage(figura, xyz, bounds, getLineEquation(U, V));
			numberOfNoLines = 0;
		} // if (nearestLine.second < MIN_ANGLE)
		else {
			numberOfNoLines++;
		}
		xyz[2] = xyz[2] + 1; // pasa a la siguiente
	} // while

	numberOfNoLines = 0;
	lastLine = firstLine;
	lastCentroid = lastCentroidAux;
	lastZ = ijk[2];
	xyz[0] = ijk[0];
	xyz[1] = ijk[1];

	// Borrar hacia abajo
	xyz[2] = ijk[2] - 1; // primera imagen
	while (xyz[2] >= bounds.MIN_Z) { // hasta llegar a la �ltima imagen
		lines[xyz[2]] = getLinesFromImage(figura, xyz[2]);
		std::pair<Line, double> nearestLine = findNearestLine(lines[xyz[2]], lastLine, lastZ, xyz[2]);
		if (nearestLine.second < MIN_ANGLE) {
			if (numberOfNoLines != 0) {
				int A[3] = { lastLine.first.x, lastLine.first.y, lastZ };
				int B[3] = { lastLine.second.x, lastLine.second.y, lastZ };
				int C[3] = { nearestLine.first.first.x, nearestLine.first.first.y, xyz[2] };
				PlaneEq P = getPlaneEquation(A, B, C);
				int xyzAux[3] = { xyz[0], xyz[1], xyz[2] + numberOfNoLines };
				for (; xyzAux[2] > xyz[2]; xyzAux[2]--) {
					if (!isAdjacent(figura, { xyzAux[0], xyzAux[1], xyzAux[2] }, MIN_WOOD, MAX_WOOD)) {
						xyzAux[0] = lastCentroid[0];
						xyzAux[1] = lastCentroid[1];
					}
					if (!isAdjacent(figura, { xyzAux[0], xyzAux[1], xyzAux[2] }, MIN_WOOD, MAX_WOOD)) {
						Coord2D newCentroid = searchInitialVoxel(figura, xyzAux, bounds, MIN_WOOD, MAX_WOOD, getLineEquationFromPlane(P, xyzAux[2]));
						xyzAux[0] = newCentroid[0];
						xyzAux[1] = newCentroid[1];
					}
					lastCentroid = regionGrowingWithLineBoundImage(figura, xyzAux, bounds, getLineEquationFromPlane(P, xyzAux[2]));
				} // for
			} // if (numberOfNoLines != 0)
			lastLine = nearestLine.first;
			lastZ = xyz[2];
			int U[3] = { lastLine.first.x, lastLine.first.y, lastZ };
			int V[3] = { lastLine.second.x, lastLine.second.y, lastZ };
			if (!isAdjacent(figura, { xyz[0], xyz[1], xyz[2] }, MIN_WOOD, MAX_WOOD)) {
				xyz[0] = lastCentroid[0];
				xyz[1] = lastCentroid[1];
			}
			if (!isAdjacent(figura, { xyz[0], xyz[1], xyz[2] }, MIN_WOOD, MAX_WOOD)) {
				Coord2D newCentroid = searchInitialVoxel(figura, xyz, bounds, MIN_WOOD, MAX_WOOD, getLineEquation(U, V));
				xyz[0] = newCentroid[0];
				xyz[1] = newCentroid[1];
			}
			lastCentroid = regionGrowingWithLineBoundImage(figura, xyz, bounds, getLineEquation(U, V));
			numberOfNoLines = 0;
		} // if (nearestLine.second < MIN_ANGLE)
		else {
			numberOfNoLines++;
		}
		xyz[2] = xyz[2] - 1; // pasa a la siguiente
	} // while
}