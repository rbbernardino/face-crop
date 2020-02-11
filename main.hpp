// #include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp> 
#include <opencv2/imgproc/imgproc.hpp>

#include <experimental/filesystem>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp> // to_upper, to_lower, split

#include "NbcRect.hpp"
#include "args.hxx"

using namespace cv;
namespace fs = std::experimental::filesystem;

//------------------------------------------------------------------------------
// -------------------- Rectangle definitions --------------------------------
enum RectSizeDirection { up, down, left, right };

//-----------------------------------------------------------------------------
//----------------- Functions Declarations --------------------------------

// default color is RED
void DrawRectangle(Mat img, NbcRect rect);

void MouseHandler(int event, int x, int y, int flags, void* userdata);

void MoveRect(NbcRect &r, int movedX, int movedY);
void MouseMoveRect(NbcRect &r, Point mouseEnd);
bool IsValidPos(NbcRect r);
bool IsValidPos(NbcRect r, int movedX, int movedY);
void SetRectPos(NbcRect &r, Point pos);
void IncreaseRectSize(RectSizeDirection direction, NbcRect &rectToCrop, int increaseUnit);
void DecreaseRectSize(RectSizeDirection direction, NbcRect &rectToCrop, int increaseUnit);

void InitializeRectangles();
void UpdateMainWindow();
void UpdateDetailWindow();

bool ToggleRect();
void UpdatePreviewWindow();
   
int CropFile(fs::path filepath);
bool isImage(fs::path fpath);
