#ifndef NBC_RECT_H
#define NBC_RECT_H

#include <opencv2/core/core.hpp>

class NbcRect: public cv::Rect {
 private:
  
 public:
  cv::Point mvStart;
  cv::Scalar color;

 public:
  void ResetStart();
  void Move(int movedX, int movedY);
  
  NbcRect();
  NbcRect(int ptX, int ptY, int width, int height, cv::Scalar color);
};

#endif
