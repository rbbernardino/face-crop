#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <array>
#include <map>
#include <fstream> // read file streams
#include <sstream> // read string stream from file

#include "Zoom.hpp"
#include "main.hpp"

#include "alphanum.hpp"

using namespace std;
using namespace cv;
namespace fs = std::experimental::filesystem;

//-----------------------------------------------------------------------------
const int recWidth = 10; // TODO option to receive as parameter
const int recHeight = 10;
const Scalar rectMovingColor = CV_RGB(144, 0, 255);
const Scalar rectResizingColor = CV_RGB(242, 245, 66);

//const Size IMG_WIN_SIZE = Size(1080, 1920);
const Size IMG_WIN_SIZE = Size(800, 800);
// const int PREVIEW_WIN_WIDTH = recWidth * 5;
const int PREVIEW_WIN_WIDTH = recWidth * 5;

// set rectangles properties
const int RECT_THICKNESS = 1;
const int RECT_LINETYPE  = 4;

const string MAIN_WINDOW_NAME = "FaceCrop";
const string DETAIL_WINDOW_NAME = "Crop Preview";

//-----------------------------------------------------------------------------
// Files definitions
const string SUFIX = "_crop";
const fs::path DONE_DIR = fs::path("done");
const fs::path LATER_DIR = fs::path("later");
const fs::path CROPPED_DIR = fs::path("cropped");
const fs::path NOFACE_DIR = fs::path("noface");
const fs::path RECTPOS_DIR = fs::path("rectpos");
const string OUT_EXT = ".png";

enum InType { file, dir } inputType;
enum NextDirection { goBack, goForward, fileRemoved, notImage };

//----------------------------------------------------------------------------
//--------------------- Global Variables ---------------------------------

// original image, shown image and marked version
Mat imgOriginal, imgMarked, previewImg;

// keeps current filename to show on image
string cur_fname;

// whether has GT file
bool hasGT;
std::map<string, cv::Rect> gtRects;
bool keepRect = false;

// whether should apply negative filter to shown marked image
bool negativeFilt = false;

// mouse related variables
bool mousePressed = false;
Point mouseStart;
int mouseMoveUnit = 1;

// keyboard related variables
bool arrowsToIncreaseSize = false;

// hold rectangles data
NbcRect rectToCrop = NbcRect(0, 0, 10, 10, rectMovingColor);

void PrintHelp(char *argv[]) {
    cout << string(argv[0]) + " -f FILE_PATH" << endl;
    cout << "    crop only one file" << endl <<endl;
  
    cout << string(argv[0]) + " -d DIRECTORY_PATH" << endl;
    cout << "    crop all files in directory" << endl;

}

// ----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    string commandsHint =
	" q - quits the program\n"
	" RET - crop the image like shown in the `Crop Preview` window\n"
	" ESC - skips the image (move to `later`)\n"
	" Arrows - move the rectangle `moveUnit` pixels\n"
	" 1, 2, 3 ... 0 - changes the `moveUnit` to 1, 5, 15 ... 500, 1000 pixels per move\n"
	" z - alternates between `Normal Mode` and `Split Crop Mode`\n"
	" SPC - move image to `no-face`\n"
	" i - invert colors (only displayed image, not the cropped result)";

    args::ArgumentParser parser("Tool to generating a face detection dataset."
				"The input and output image may be in any format supported by OpenCV.",
				commandsHint);
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<std::string> pImagePath(parser, "file", "The input image", {'f'});
    args::ValueFlag<std::string> pDirectoryPath(parser, "directory", "The input directory", {'d'});
    args::ValueFlag<std::string> pGTFilePath(parser, "gt", "Ground-truth TSV file with \"filename, x, y, width, height\"", {'g'});
    args::Flag pKeepRect(parser, "keepRect", "Keep rectangle if no GT found or no GT file provided", {'k'});

    try {
	parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
	std::cout << parser;
	return 0;
    }
    catch (args::ParseError e)
    {
	std::cerr << e.what() << std::endl;
	std::cerr << parser;
	return 1;
    }

    if((pImagePath && pDirectoryPath) || (!pImagePath && !pDirectoryPath)) {
	cout << "ERROR! Please, specify either a file path OR a directory path." << endl;
	cout << parser;
	return 1;
    }

    if(pImagePath) inputType = InType::file;
    if(pDirectoryPath) inputType = InType::dir;
    if(pKeepRect) keepRect = true;

    if(pGTFilePath) {
	hasGT = true;
	string line;
	std::ifstream infile(args::get(pGTFilePath));
	while (std::getline(infile, line))
	{
	    std::istringstream iss(line);
	    string imagename;
	    int x, y, width, height;
	    if(!(iss >> imagename >> x >> y >> width >> height)) { break; }
	    gtRects[imagename] = cv::Rect(x, y, width, height);
	}
    }

    // open the main window and set mouse input detection
    namedWindow(MAIN_WINDOW_NAME, WINDOW_NORMAL);
    resizeWindow(MAIN_WINDOW_NAME, IMG_WIN_SIZE.width, IMG_WIN_SIZE.height);
    setMouseCallback(MAIN_WINDOW_NAME, MouseHandler, NULL);

    // open crop preview window
    namedWindow(DETAIL_WINDOW_NAME, WINDOW_NORMAL);
    double resizeFactor = PREVIEW_WIN_WIDTH/(double)recWidth; 
    resizeWindow(DETAIL_WINDOW_NAME, (int)recWidth*resizeFactor, (int)recHeight*resizeFactor);

    // initialize main image and set initial crop rectangle position
    imgMarked = Mat(IMG_WIN_SIZE, CV_8UC3);
    InitializeRectangles();
  
    if(inputType == InType::file) {
	fs::path fpath = fs::path(args::get(pImagePath));
	CropFile(fpath);
    }
    else {
	fs::path dirpath = fs::path(args::get(pDirectoryPath));

	// copy all paths to vector and sort them
	vector<fs::path> all_paths;
	copy(fs::directory_iterator(dirpath), fs::directory_iterator(), back_inserter(all_paths));
	sort(all_paths.begin(), all_paths.end());
	sort(all_paths.begin(), all_paths.end(), doj::alphanum_less<std::string>());
	
	for(int i = 0; i < all_paths.size();) {
	    fs::path fpath = all_paths[i];
	    try{
		NextDirection result = (NextDirection) CropFile(fpath);

		if(result == fileRemoved || result == notImage) {
		    // remove fpath from list
		    all_paths.erase(all_paths.begin() + i);
		    if(i >= all_paths.size() && (i-1 >= 0))
			i--;
		}
		else if(result == goBack){
		    // sets 'i' to the image that comes before current one
		    if(i - 1 >= 0)
			i--;
		} else { // goForward
		    i++;
		}
	    } catch(exception& e){
		cout << "\nFatal ERROR!" << endl;
		cout << "----------------" << endl;
		cout << e.what() << endl;
		cout << "----------------" << endl;
		cout << "Filename: " << fpath.filename() << endl;
		exit(1);
	    }
	  
	    // avoid close at the end
	    if(i == all_paths.size() && (i-1) >= 0)
		i--;
	}
    }
    return 0;
}

bool isImage(fs::path fpath) {
    string lowExt = fpath.extension().string();
    boost::to_lower(lowExt);
    return
	lowExt == ".jpeg" ||
	lowExt == ".jpg" ||
	lowExt == ".png" ||
	lowExt == ".tif" ||
	lowExt == ".tiff" ||
	lowExt == ".bmp";
}

int CropFile(fs::path IN_fpath) {
    if(!isImage(IN_fpath))
	return NextDirection::notImage;
    
    if(!exists(IN_fpath) || is_directory(IN_fpath)) {
	cout << "ERROR! specified file " << IN_fpath <<" doesn't exist!!" <<endl;
	return -1;
    }
    
    // split input file path string (parent and name)
    fs::path IN_fname = IN_fpath.stem();
    fs::path IN_fdir  = IN_fpath.parent_path();
    string IN_fext  = IN_fpath.extension().string();

    // saves name to update on shown image part (print on image)
    cur_fname = IN_fname.string();
    cout << "Current file: " << IN_fname; cout.flush();

    // read input image and set up the main window
    imgOriginal = imread(IN_fpath.string());
    imgMarked = imgOriginal.clone();
    InitializeRectangles();
    UpdateMainWindow();    // will open the main window
    UpdatePreviewWindow(); // will open crop preview window
  
    // waits for user keyboard input
    bool done=false, do_crop=false, crop_later=false, no_face=false, ignore=false, back=false;
    do {
	int key = waitKey();

	switch(key) {
	case '\r': // RET
	    done = true;
	    do_crop = true;
	    break;

	case '\n': // RET
	    done = true;
	    do_crop = true;
	    break;
	  
	case 27: // ESC
	    done=true;
	    crop_later=true;
	    break;

	case 'n': // NEXT without doing nothing
	    done=true;
	    ignore=true;
	    break;

	case 'p': // PREVIOUS without doing nothing
	    done=true;
	    ignore=true;
	    back=true;
	    break;

	case 'i': // invert colors
	    negativeFilt = !negativeFilt;
	    UpdateMainWindow();
	    break;
	  
	case 32: // SPC
	    done = true;
	    no_face = true;
	    UpdateMainWindow();
	    UpdatePreviewWindow();
	    break;

	    // troca orientação do retângulo selecionado
	case '\t': // TAB
	    arrowsToIncreaseSize = !arrowsToIncreaseSize;
	    UpdateMainWindow();
	    UpdatePreviewWindow();
	    break;

	    // alterna entre modes
	// case 'z': // z // TODO 
	//     InitializeRectangles();
	//     UpdateMainWindow();
	//     UpdatePreviewWindow();
	//     break;

	case 82: // UP
	    if(arrowsToIncreaseSize) {
		DecreaseRectSize(RectSizeDirection::up, rectToCrop, mouseMoveUnit);
	    } else {
		MoveRect(rectToCrop, 0, -1 * mouseMoveUnit);
	    }
	    UpdateMainWindow();
	    UpdatePreviewWindow();
	    break;

	case 84: // DOWN
	    if(arrowsToIncreaseSize) {
		IncreaseRectSize(RectSizeDirection::down, rectToCrop, mouseMoveUnit);
	    } else {
		MoveRect(rectToCrop, 0, +1 * mouseMoveUnit);
	    }
	    UpdateMainWindow();
	    UpdatePreviewWindow();
	    break;

	case 81: // LEFT
	    if(arrowsToIncreaseSize) {
		DecreaseRectSize(RectSizeDirection::left, rectToCrop, mouseMoveUnit);
	    } else {
		MoveRect(rectToCrop, -1 * mouseMoveUnit, 0);
	    }
	    UpdateMainWindow();
	    UpdatePreviewWindow();
	    break;

	case 83: // RIGHT
	    if(arrowsToIncreaseSize) {
		IncreaseRectSize(RectSizeDirection::right, rectToCrop, mouseMoveUnit);
	    } else {
		MoveRect(rectToCrop, +1 * mouseMoveUnit, 0);
	    }
	    UpdateMainWindow();
	    UpdatePreviewWindow();
	    break;

	case -1: // system 'x' (close) window button
	case 'x':
	    exit(0);
	    break;

	case '+':
	    if(Zoom::Increase()) {
		UpdateMainWindow();
	    }
	    break;
	case '=':
	    if(Zoom::Increase()) {
		UpdateMainWindow();
	    }
	    break;

	case '-':
	    if(Zoom::Decrease()) {
		UpdateMainWindow();
	    }
	    break;	  

	case '[': // ZOOM UP
	    if(Zoom::MoveUp()) {
		UpdateMainWindow();
	    }
	    break;

	case ']': // ZOOM DOWN
	    if(Zoom::MoveDown()) {
		UpdateMainWindow();
	    }
	    break;

	case 49: mouseMoveUnit = 1; break; // 1
	case 50: mouseMoveUnit = 5; break; // 2
	case 51: mouseMoveUnit = 15; break; // 3
	case 52: mouseMoveUnit = 30; break; // 4
	case 53: mouseMoveUnit = 50; break; // 5
	case 54: mouseMoveUnit = 80; break; // 6
	case 55: mouseMoveUnit = 200; break; // 7
	case 56: mouseMoveUnit = 350; break; // 8
	case 57: mouseMoveUnit = 500; break; // 9
	case 48: mouseMoveUnit = 1000; break; // 0
	  
	default:
	    cout << "Tecla '" << key << "' pressionada" << endl;
	    continue;
	}
    }while(!done);

    if(do_crop) {
	// crop the image
	Mat imgCropped;
	imgCropped = previewImg;

	// saves cropped image to file
	string OUT_fname = IN_fname .concat( SUFIX + OUT_EXT ).string();
	fs::path OUT_fpath = IN_fdir / CROPPED_DIR / OUT_fname;
	if(!fs::exists( IN_fdir / CROPPED_DIR ))
	    fs::create_directory( IN_fdir / CROPPED_DIR );
	imwrite(OUT_fpath.string(), imgCropped);

	// move original image to "done" folder
	if(!fs::exists( IN_fdir / DONE_DIR ))
	    fs::create_directory( IN_fdir / DONE_DIR );
	fs::rename(IN_fpath, IN_fdir / DONE_DIR / IN_fpath.filename());

	// saves crop position to file
	if(!fs::exists( IN_fdir / RECTPOS_DIR ))
	    fs::create_directory( IN_fdir / RECTPOS_DIR );
	fs::path rectPos_fname = fs::path(cur_fname + "_cropRect.tsv");
	fs::path rectPos_fpath = IN_fdir / RECTPOS_DIR / rectPos_fname;
	std::ofstream rectPosOutFile(rectPos_fpath.string());
	rectPosOutFile << "filename,x,y,width,height\n"
		       << cur_fname << ","
		       << rectToCrop.x << ","
		       << rectToCrop.y << ","
		       << rectToCrop.width << ","
		       << rectToCrop.height << endl;
	
	cout << " | " << "cropped!" << endl;
	return NextDirection::fileRemoved;
    }
    else if(crop_later){
	// moves original image to "later" folder
	if(!fs::exists( IN_fdir / LATER_DIR ))
	    fs::create_directory( IN_fdir / LATER_DIR );
	fs::rename(IN_fpath, IN_fdir / LATER_DIR / IN_fpath.filename());

	cout << " | " << "left for later..." << endl;
	return NextDirection::fileRemoved;
    }
    else if(no_face){
	// moves original image to "noface" folder
	if(!fs::exists( IN_fdir / NOFACE_DIR ))
	    fs::create_directory( IN_fdir / NOFACE_DIR );
	fs::rename(IN_fpath, IN_fdir / NOFACE_DIR / IN_fpath.filename());

	cout << " | " << "moved to \"noface\" dir..." << endl;
	return NextDirection::fileRemoved;
    }
    else if(back) {
	cout << " | " << "go back one image!" << endl;
  	return NextDirection::goBack;
    }
    else if(ignore){
	cout << " | " << "ignored!" << endl;
	return NextDirection::goForward;
    }
}


// -----------------------------------------------------------------------------
// ------------------- Mouse Handling Functions --------------------------------

void MouseHandler(int event, int x, int y, int flags, void* userdata) {
    switch(event) {
    case EVENT_RBUTTONDOWN:
	mouseStart = Point(x, y);
	SetRectPos(rectToCrop, Point(x, y));
	UpdateMainWindow();
	break;

    case EVENT_RBUTTONUP:
	mouseStart = Point(x, y);
	rectToCrop.ResetStart();
	MouseMoveRect(rectToCrop, Point(x, y));
	UpdateMainWindow();
	mousePressed = false;
	break;

    case EVENT_LBUTTONDOWN:
	mouseStart = Point(x, y);
	mousePressed = true;
	UpdateMainWindow();
	break;

    case EVENT_LBUTTONUP:
	mousePressed = false;
	rectToCrop.ResetStart();
	UpdateMainWindow();
	break;

    case EVENT_MOUSEMOVE:
	if(mousePressed) {
	    MouseMoveRect(rectToCrop, Point(x, y));
	    UpdateMainWindow();
	}
	break;

    default:
	break;
    }
}

// ----------------------------------------------------------------------------
// ----------------- Maintain Rectangles Functions ----------------------------

// the are drawn from the upper left corner point
void InitializeRectangles() {
    int ptX, ptY, recWidth1, recHeight1;
    bool faceRectGTExists = false;
    if(hasGT) {
	if(gtRects.find(cur_fname) != gtRects.end()) {
	    ptX = gtRects[cur_fname].x;
	    ptY = gtRects[cur_fname].y;
	    recWidth1 = gtRects[cur_fname].width;
	    recHeight1 = gtRects[cur_fname].height;
	    faceRectGTExists = true;
	}
    }

    if(!faceRectGTExists) {
	if(!keepRect) {
	    ptX = 0, ptY = 0;
	    recWidth1 = 10, recHeight1 = 10;
	} else {
	    ptX = rectToCrop.x, ptY = rectToCrop.y;
	    recWidth1 = rectToCrop.width, recHeight1 = rectToCrop.height;
	}
    }

    rectToCrop = NbcRect(ptX, ptY, recWidth1, recHeight1, rectMovingColor);
    if(!IsValidPos(rectToCrop)) {
	cout << "\nERROR: GT file contains face rect outside image area: "
	     << cur_fname
	     << "rect (" << ptX << ", " << ptY << ", " << recWidth1 << ", " << recHeight1 << ")"
	     << endl;
	exit(1);
    }
}

void UpdateMainWindow() {
    // highlights rectangle to identify arrow keys mode
    if(arrowsToIncreaseSize)
	rectToCrop.color = rectResizingColor;
    else
	rectToCrop.color = rectMovingColor;

    //---------------------------------
    // first updates imgMarked
    imgMarked = imgOriginal.clone();
    DrawRectangle(imgMarked, rectToCrop);
    Mat imgView = Zoom::Apply(imgMarked);

    // prints filename on upper left corner
    cv::putText(imgView, cur_fname, Point(5,35-(5*Zoom::GetZoomState())), FONT_HERSHEY_DUPLEX, 0.35*abs(4-Zoom::GetZoomState()), CV_RGB(0, 0, 0));

    // apply appropriate filters and update windows
    if(negativeFilt)
	cv::bitwise_not(imgView, imgView);
    imshow(MAIN_WINDOW_NAME, imgView);
    UpdatePreviewWindow();
}

bool IsValidPos(NbcRect r) {
    bool notValidX = r.x + r.width  >= imgMarked.cols || r.x < 0;
    bool notValidY = r.y + r.height >= imgMarked.rows || r.y < 0;

    if(notValidX || notValidY)
	return false;

    return true;
}

bool IsValidPos(NbcRect r, int movedX, int movedY) {
    // rectangle internal padding in order to surround crop area
    // movedX += 1;
    // movedY += 1;

    bool notValidX = r.mvStart.x + movedX + r.width  >= imgMarked.cols || r.mvStart.x + movedX < 0;
    bool notValidY = r.mvStart.y + movedY + r.height >= imgMarked.rows || r.mvStart.y + movedY < 0;

    if(notValidX || notValidY)
	return false;

    return true;
}

void MoveRect(NbcRect &r, int movedX, int movedY) {
    if(IsValidPos(r, movedX, movedY)) {
	r.Move(movedX, movedY);
	r.ResetStart();
    }
}

void MouseMoveRect(NbcRect &r, Point mouseEnd) {
    // calcula amplitude do movimento
    int movedX = mouseEnd.x - mouseStart.x;
    int movedY = mouseEnd.y - mouseStart.y;

    // Atualiza posições dos retângulos
    if(IsValidPos(r, movedX, movedY))
	r.Move(movedX, movedY);
}

void SetRectPos(NbcRect &r, Point pos) {
    int newX = pos.x - r.width/2;
    int newY = pos.y - r.height/2;

    // check and fix X position
    if(newX < 0) r.x = 1;
    else if(newX + r.width >= imgMarked.cols) r.x = (imgMarked.cols-1) - r.width;
    else r.x = newX;

    // check and fix Y position
    if(newY < 0) r.y = 1;
    else if(newY + r.height >= imgMarked.rows) r.y = (imgMarked.rows-1) - r.height;
    else r.y = newY;

    r.ResetStart();
}

void IncreaseRectSize(RectSizeDirection direction, NbcRect &r, int increaseUnit) {
    NbcRect newRect = r;
    switch(direction) {
    case RectSizeDirection::right:
	newRect.width += increaseUnit;
	break;
    case RectSizeDirection::down:
	newRect.height += increaseUnit;
	break;
    }
    if(IsValidPos(newRect)) {
	r.width = newRect.width;
	r.height = newRect.height;
    }
}

void DecreaseRectSize(RectSizeDirection direction, NbcRect &r, int increaseUnit) {
    NbcRect newRect = r;
    switch(direction) {
    case RectSizeDirection::left:
	newRect.width -= increaseUnit;
	break;
    case RectSizeDirection::up:
	newRect.height -= increaseUnit;
	break;
    }
    if(IsValidPos(newRect)) {
	r.width = newRect.width;
	r.height = newRect.height;
    }
}

void UpdatePreviewWindow() {
    double resizeFactor = PREVIEW_WIN_WIDTH/(double)recWidth; 
    Size newSize = Size();

    previewImg = imgMarked(rectToCrop);
    newSize.width = rectToCrop.width  * resizeFactor;
    newSize.height = rectToCrop.height  * resizeFactor;
  
    // resizeWindow(DETAIL_WINDOW_NAME, newSize.width, newSize.height);
    imshow(DETAIL_WINDOW_NAME, previewImg);
}

// draw a rectangle surroung received rectangle rec on img
void DrawRectangle(Mat img, NbcRect rec) {
    // adjust size and position
    int rect_padding = RECT_THICKNESS;

    rec.y -= rect_padding;
    rec.x -= rect_padding;
    rec.width  += rect_padding*2;
    rec.height += rect_padding*2;

    cv::rectangle(img, rec, rec.color, RECT_THICKNESS, RECT_LINETYPE, 0);
}
