#include "Zoom.hpp"
// #include <stdlib.h>
// #include <iostream>

namespace Zoom
{
  // Zoom namespace local functions 
  int CalcYPosition(int rows);
  int MaxPosition();
  float MoveFactor();
  
  enum Zoom { normal, medium, large, veryLarge, ultraLarge } zoomState = normal;
  int GetZoomState() { return (int) zoomState; }

  // (1)x3 = <3> --> 1/<3> mv factor --> pos 0~(1-<3>)
  int zoomPosition = 0; // second position, middle for medium

  // start on medium (1)
  int zoomMoveAux = 1; // 1~2~4~8 -> generate moveFactor, 1/3, 1/6, 1/12 ...
  
  Mat Apply(Mat img) {
	cv::Point ref_point;
	cv::Size  crop_size;

	int imgCols = img.cols;
	int imgRows = img.rows;

	int newX = 0, newY = 0;
	int newCols = 0;
  
	switch(zoomState) {
	case normal:
	  // restore to original
	  newY = 0;
	  newCols = imgCols;
	  crop_size = Size(imgCols, imgRows);
	  break;

	case medium:
	  // 3/4 + 1/8 = 7/8
	  newCols = imgCols*9/10;

	  crop_size = Size  (newCols, imgRows*1/3);
	  break;
	  
	case large:
	  // 2/4 - 1/16 = 7/16
	  newCols = imgCols*10/17;

	  crop_size = Size  (newCols, imgRows*2/9);
	  break;

	case veryLarge:
	  // 1/4 - 1/32 = 7/32
	  newCols = imgCols*13/32;
	
	  crop_size = Size  (newCols, imgRows*2/13);
	  break;

	case ultraLarge:
	  // 1/8 - 1/64 = 7/64
	  newCols = imgCols*7/64;
	
	  crop_size = Size  (newCols, imgRows*1/24);
	  break;
	}

	// metade da sobra em cada lado = centraliza
	newX = (imgCols-newCols)/2;
	newY = CalcYPosition(imgRows);
	ref_point = cv::Point (newX, newY);

	return(img(cv::Rect(ref_point, crop_size)).clone());
  }

  bool Increase() {
	if(zoomState < ultraLarge) {
	  zoomState = (Zoom) ((int)zoomState + 1);

	  if(zoomState == normal)
		zoomMoveAux = 1;
	  else
		zoomMoveAux *= 2;

	  zoomPosition = zoomPosition*2 +1;

	  return true;
	}
	else
	  return false;
  }

  bool Decrease() {
	if(zoomState > normal) {
	  zoomState = (Zoom) ((int)zoomState - 1);

	  if(zoomState == normal)
		zoomMoveAux = 1;
	  else
		zoomMoveAux /= 2;

	  zoomPosition /= 2;
	  
	  return true;
	}
	else
	  return false;
  }

  bool MoveUp() {
	if(zoomPosition > 0 && zoomState != normal) {
	  zoomPosition--;
	  return true;
	}
	else
	  return false;
  }

  bool MoveDown() {
	if(zoomPosition < MaxPosition() && zoomState != normal) {
	  zoomPosition++;
	  return true;
	}
	else
	  return false;
  }

  int CalcYPosition(int rows) {
	switch(zoomState) {
	case normal: return 0;
	default:     return (rows * MoveFactor()) * zoomPosition;
	}
  }

  int MaxPosition()   { return (zoomMoveAux*3)-2; }
  float MoveFactor()  { return 1.0/(zoomMoveAux*3); }
}
