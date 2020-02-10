#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp> 
#include <opencv2/imgproc/imgproc.hpp>

#include "NbcRect.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp> //toupper

using namespace boost::filesystem;

//------------------------------------------------------------------------------
// -------------------- Rectangle definitions --------------------------------
struct Offset {
  int left, right, up, down;
  Offset(int l, int r, int u, int d) { left = l; right = r; up = u; down = d; }
};

//-----------------------------------------------------------------------------
//----------------- Functions Declarations --------------------------------

// default color is RED
void DrawRectangle(Mat img, NbcRect rect);
bool IsPortrait(Mat img);
Mat FixMatOrientation(Mat rImg);

void MouseHandler(int event, int x, int y, int flags, void* userdata);

void MoveRect(NbcRect *r, int movedX, int movedY);
void MouseMoveRect(NbcRect *r, Point mouseEnd);
bool IsValidPos(NbcRect r, int movedX, int movedY);
void SetRectPos(NbcRect *r, Point pos);
void ToggleRectOrientation(NbcRect *r);

void InitializeRectangles();
void UpdateMainWindow();
void UpdateDetailWindow();

bool ToggleRect();
void SelectRectangle(NbcRect *r);
void SelectRectangle(int x, int y);
void UpdatePreviewWindow();
   
int CropFile(path filepath);
bool isImage(path fpath);
