#ifndef ZOOM_HPP
#define ZOOM_HPP

#include <opencv2/core/core.hpp>

using namespace cv;

namespace Zoom {
  Mat Apply(Mat imgOriginal);
  bool Increase();
  bool Decrease();
  bool MoveUp  ();
  bool MoveDown();
  int GetZoomState();
};

#endif
