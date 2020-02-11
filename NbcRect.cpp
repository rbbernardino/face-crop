#include "NbcRect.hpp"

NbcRect::NbcRect() : cv::Rect(){}

NbcRect::NbcRect(int ptX, int ptY, int width, int height, cv::Scalar color) : cv::Rect(ptX, ptY, width, height) {
  mvStart = cv::Point(ptX, ptY);
  this->color = color;
}

void NbcRect::Move(int movedX, int movedY) {
  this->x = mvStart.x + movedX;
  this->y = mvStart.y + movedY;
}

void NbcRect::ResetStart() {
  mvStart = cv::Point(this->x, this->y);
}
