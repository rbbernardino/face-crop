#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <array>

#include "Zoom.hpp"
#include "main.h"

using namespace std;
using namespace cv;

//-----------------------------------------------------------------------------
const int recWidth = 50;
const int recHeight = 20;
const Scalar defaultColor  = CV_RGB(141, 139, 155);
const Scalar selectedColor = CV_RGB(144, 0, 255);

//const Size IMG_WIN_SIZE = Size(1080, 1920);
const Size IMG_WIN_SIZE = Size(600, 800);
const int PREVIEW_WIN_WIDTH = recWidth * 5;

bool MULTI_CROP_MODE = false;
const int MCROP_RECT_NUM = 6; // must be even number! 2, 4, 6, 8...

enum CDir { hor, ver } cropDirection = hor;

// LEFT, RIGTH, UP, DOWN
const Offset offset_S  (  0,  0,  0, 25 );
const Offset offset_SW ( 25,  0,  0, 10 );
const Offset offset_N  (  0,  0, 20,  0 );
const Offset offset_NW ( 25,  0, 25,  0 );
const Offset offset_E  (  0,  10, 0,  0 );

// set active rectangles
const bool showS  = false;
const bool showSW = false;
const bool showN  = true;
const bool showNW = false;
const bool showE  = true;

// set rectangles properties
const int RECT_THICKNESS = 1;
const int RECT_LINETYPE  = 4;

const string MAIN_WINDOW_NAME = "CropNabuco";
// const string DETAIL_WINDOW_NAME = "Crop Preview";

//-----------------------------------------------------------------------------
// Files definitions
const string SUFIX = "_crop";
const path DONE_DIR = path("done");
const path LATER_DIR = path("later");
const path CROPPED_DIR = path("cropped");
const path RECTPOS_DIR = path("rectpos");
const string OUT_EXT = ".bmp";

enum InType { file, dir } inputType;
enum NextDirection { goBack, goForward, fileRemoved, notImage };

//----------------------------------------------------------------------------
//--------------------- Global Variables ---------------------------------

// original image, shown image and marked version
Mat imgOriginal, imgMarked, previewImg;

// keeps current filename to show on image
string cur_fname;

// whether should apply negative filter to shown marked image
bool negativeFilt = false;

// mouse related variables
bool mousePressed = false;
Point mouseStart;
int mouseMoveUnit = 1;

// hold rectangles data
NbcRect rectS, rectSW;
NbcRect rectN, rectNW;
NbcRect rectE;

// multi rectangle mode
array<NbcRect, MCROP_RECT_NUM> rectList;

// default is rectN, as seen at the end of InitializeRectangles function
NbcRect *selectedRect = NULL;

void PrintHelp(char *argv[]) {
  cout << string(argv[0]) + " -f FILE_PATH" << endl;
  cout << "    crop only one file" << endl <<endl;
  
  cout << string(argv[0]) + " -d DIRECTORY_PATH" << endl;
  cout << "    crop all files in directory" << endl;
}

// ----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  if(argc < 3) {
	cout << "ERROR! Please, specify type AND input (file/directory)." << endl << endl;
	PrintHelp(argv);
	return 1;
  }

  if     (string(argv[1]) == "-f" ) inputType = InType::file;
  else if(string(argv[1]) == "-d" ) inputType = InType::dir;
  else {
	cout << "ERROR! Input Type should be either '-f' or '-d'" << endl;
	PrintHelp(argv);
	return 1;
  }

  if(MCROP_RECT_NUM % 2 != 0) {
	cout << "ERROR! Number of rectangles for Multi Crop must be even!" << endl;
	cout << "That's hardcoded, please fix it and recompile..." << endl;
  }
  
  // abre janela de imagem e configura recepcao de dados do mouse
  namedWindow(MAIN_WINDOW_NAME, WINDOW_NORMAL | CV_GUI_NORMAL);
  resizeWindow(MAIN_WINDOW_NAME, IMG_WIN_SIZE.width, IMG_WIN_SIZE.height);
  setMouseCallback(MAIN_WINDOW_NAME, MouseHandler, NULL);

  // abre janela da previsão de corte da imagem
  // namedWindow(DETAIL_WINDOW_NAME, WINDOW_NORMAL | CV_GUI_NORMAL);
  // double resizeFactor = PREVIEW_WIN_WIDTH/(double)recWidth; 
  // resizeWindow(DETAIL_WINDOW_NAME, (int)recWidth*resizeFactor, (int)recHeight*resizeFactor);

  // inicializ img e configura posição inicial dos retângulos
  imgMarked = Mat(IMG_WIN_SIZE, CV_8UC3);
  InitializeRectangles();
  
  if(inputType == InType::file) {
	path fpath = path(argv[2]);
	CropFile(fpath);
  }
  else {
	path dirpath = path(argv[2]);

	// store paths, so we can sort them later
	vector<path> all_paths;

	// copy all paths to vector and sort them
	copy(directory_iterator(dirpath), directory_iterator(), back_inserter(all_paths));
	sort(all_paths.begin(), all_paths.end());
	
	for(int i = 0; i < all_paths.size();) {
	  path fpath = all_paths[i];
	  try{
		NextDirection result = (NextDirection) CropFile(fpath);

		if(result == fileRemoved || result == notImage) {
		  // remove fpath from list
		  all_paths.erase(all_paths.begin() + i);
		  if(i - 1 >= 0)
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

// funcao compare() retorna invertido do C++ (true = 1)
bool isImage(path fpath) {
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

int CropFile(path IN_fpath) {
  if(!isImage(IN_fpath))
	 return NextDirection::notImage;

  if(!exists(IN_fpath) || is_directory(IN_fpath)) {
	cout << "ERROR! specified file " << IN_fpath <<" doesn't exist!!" <<endl;
	return -1;
  }
  
  // usando biblioteca boost::filesystem
  // separa partes do caminho para o arquivo de entrada
  path IN_fname = IN_fpath.stem();
  path IN_fdir  = IN_fpath.parent_path();

  // saves name to update on shown image part
  cur_fname = IN_fname.string();
  
  // imprime nome do arquivo atual
  cout << "Current file: " << IN_fname; cout.flush();
  
  // apenas extensão é string
  string IN_fext  = IN_fpath.extension().string();

  // lê arquivo de entrada
  imgOriginal = imread(IN_fpath.string());

  // inicializa marked image
  imgMarked = imgOriginal.clone();

  // configura retangulos para essa imagem
  InitializeRectangles();

  // atualiza retangulos em imgMarked e já atualiza/abre janela da imagem com retangulos
  UpdateMainWindow();

  // a atualizacao da imagem agora é feita na função UpdateMainWindow()...
  // imshow(MAIN_WINDOW_NAME, imgMarked);

  // atualiza janela de previsao de corte
  UpdatePreviewWindow();
  
  // aguarda usuário confirmar ou cancelar
  bool done=false, do_crop=false, crop_later=false, ignore=false, back=false;
  do {
	int key = waitKey();

	switch(key) {
	case '\r': // enter
	  done = true;
	  do_crop = true;
	  break;

	case '\n': // enter
	  done = true;
	  do_crop = true;
	  break;
	  
	case 27: // esc
	  done=true;
	  crop_later=true;
	  break;

	case 'n': // esc
	  done=true;
	  ignore=true;
	  break;

	case 'p': // esc
	  done=true;
	  ignore=true;
	  back=true;
	  break;

	case 'i': // invert colors
	  negativeFilt = !negativeFilt;
	  UpdateMainWindow();
	  break;
	  
	  // troca entre retangulos do split mode
	  // alterna entre retangulos do multi mode
	case 32: // SPC
	  if(ToggleRect()) {
		UpdateMainWindow();
		UpdatePreviewWindow();
	  }
	  break;

	  // troca orientação do retângulo selecionado
	case '\t': // TAB
	  ToggleRectOrientation(selectedRect);
	  UpdateMainWindow();
	  UpdatePreviewWindow();
	  break;

	  // alterna entre modes
	case 'z': // z
	  if(MULTI_CROP_MODE) {
		MULTI_CROP_MODE = false;
	  }
	  else {
		MULTI_CROP_MODE = true;
	  }
	  InitializeRectangles();
	  UpdateMainWindow();
	  UpdatePreviewWindow();
	  break;

	case 82: // UP
	  MoveRect(selectedRect, 0, -1 * mouseMoveUnit);
	  UpdateMainWindow();
	  UpdatePreviewWindow();
	  break;

	case 84: // DOWN
	  MoveRect(selectedRect, 0, +1 * mouseMoveUnit);
	  UpdateMainWindow();
	  UpdatePreviewWindow();
	  break;

	case 81: // LEFT
	  MoveRect(selectedRect, -1 * mouseMoveUnit, 0);
	  UpdateMainWindow();
	  UpdatePreviewWindow();
	  break;

	case 83: // RIGHT
	  MoveRect(selectedRect, +1 * mouseMoveUnit, 0);
	  UpdateMainWindow();
	  UpdatePreviewWindow();
	  break;

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

	case 's': SelectRectangle(&rectS); UpdateMainWindow(); break;
	case 'w': SelectRectangle(&rectN); UpdateMainWindow(); break;
	// case  'a': SelectRectangle(&rectS); UpdateMainWindow(); break;
	case 'd': SelectRectangle(&rectE); UpdateMainWindow(); break;
	  
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
	if(previewImg.rows > previewImg.cols)
	  imgCropped = previewImg.t();
	else
	  imgCropped = previewImg;

	// prepara nome do arquivo de saída
	string OUT_fname = IN_fname .concat( SUFIX + OUT_EXT ).string();

	// gera caminho com nome para arquivo de saída
	path OUT_fpath = IN_fdir / CROPPED_DIR / OUT_fname;

	// Save texture in "cropped" folder
	if(!boost::filesystem::exists( IN_fdir / CROPPED_DIR ))
	  boost::filesystem::create_directory( IN_fdir / CROPPED_DIR );

	// escreve arquivo de saída
	imwrite(OUT_fpath.string(), imgCropped);

	// create done folder if doens't exist yet
	if(!boost::filesystem::exists( IN_fdir / DONE_DIR ))
	  boost::filesystem::create_directory( IN_fdir / DONE_DIR );

	// move original image to "done" folder
	boost::filesystem::rename(IN_fpath, IN_fdir / DONE_DIR / IN_fpath.filename());

	if(MULTI_CROP_MODE) {
	  // create done folder if doens't exist yet
	  if(!boost::filesystem::exists( IN_fdir / RECTPOS_DIR ))
		boost::filesystem::create_directory( IN_fdir / RECTPOS_DIR );

	  // salva posicao dos retangulos
	  path rectPos_fname = path(cur_fname + "_RectPos.csv");
	  path rectPos_fpath = IN_fdir / RECTPOS_DIR / rectPos_fname;
	  std::ofstream rectPos(rectPos_fpath.string());
	  // number of rects
	  rectPos << rectList.size() << endl;
	  // rects size
	  rectPos << recWidth << "," << recHeight << endl;
	  // rects positions
	  for(NbcRect r : rectList) {
		rectPos << r.x << "," << r.y << endl;
	  }
	}
	// imprime resultado
	cout << " | " << "cropped!" << endl;

	return NextDirection::fileRemoved;
  }
  else if(crop_later){
	// create later folder if doesn't exist yet
	if(!boost::filesystem::exists( IN_fdir / LATER_DIR ))
	  boost::filesystem::create_directory( IN_fdir / LATER_DIR );

	// move to later folder
	boost::filesystem::rename(IN_fpath, IN_fdir / LATER_DIR / IN_fpath.filename());

	// imprime resultado
	cout << " | " << "left for later..." << endl;

	return NextDirection::fileRemoved;
  }
  else if(back) {
	// imprime resultado
	cout << " | " << "go back one image!" << endl;

  	return NextDirection::goBack;
  }
  else if(ignore){
	// imprime resultado
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

	SetRectPos(selectedRect, Point(x, y));
	// mousePressed = true;
	// SelectRectangle(x, y);
	// MouseMoveRect(selectedRect, Point(x, y));
	
	UpdateMainWindow();
	break;

  case EVENT_RBUTTONUP:
	mouseStart = Point(x, y);
	
	// updates new final rectangle location
	rectS.ResetStart();
	rectSW.ResetStart();
	rectN.ResetStart();
	rectNW.ResetStart();
	rectE.ResetStart();

	MouseMoveRect(selectedRect, Point(x, y));
	UpdateMainWindow();
	
	mousePressed = false;
	break;

  case EVENT_LBUTTONDOWN:
	mouseStart = Point(x, y);
	mousePressed = true;

	// if(MULTI_CROP_MODE){
	  SelectRectangle(x, y);
	// }
	// else
	  // SetRectPos(selectedRect, Point(x, y)); // pos of center of rect should be

	UpdateMainWindow();
	break;

  case EVENT_LBUTTONUP:
	break;

  case EVENT_MOUSEMOVE:
	if(mousePressed) {
	  MouseMoveRect(selectedRect, Point(x, y));
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
  int ptX, ptY;

  //--------------------------------------------------
  //--------------- Normal Mode ----------------------
  // South
  ptX = imgMarked.cols/2 - recWidth/2 + offset_S.left - offset_S.right;
  ptY = (imgMarked.rows-1) - recHeight - offset_S.down;
  rectS = NbcRect(ptX, ptY, recWidth, recHeight, defaultColor);

  // SouthWest
  ptX = offset_SW.left;
  ptY = (imgMarked.rows-1) - recHeight - offset_SW.down;
  rectSW = NbcRect(ptX, ptY, recWidth, recHeight, defaultColor);

  // North
  ptX = imgMarked.cols/2 - recWidth/2 + offset_N.left - offset_N.right;
  ptY = 0 + offset_N.up;
  rectN = NbcRect(ptX, ptY, recWidth, recHeight, defaultColor);

  // NorthWest
  ptX = 0 + offset_NW.left;
  ptY = 0 + offset_NW.up;
  rectNW = NbcRect(ptX, ptY, recWidth, recHeight, defaultColor);

  // East - rotated!
  ptX = (imgMarked.cols-1) - recHeight - offset_E.right;
  ptY = imgMarked.rows/2 - recWidth/2;
  rectE = NbcRect(ptX, ptY, recHeight, recWidth, defaultColor);

  //--------------------------------------------------
  //---------------- Multi Mode ----------------------
  // First the same as North of normal mode
  ptX = imgMarked.cols/2 - recWidth/2;
  ptY = 0 + offset_N.up;
  rectList[0] = NbcRect(ptX, ptY, recWidth, recHeight, defaultColor);

  NbcRect prevRect = rectList[0];
  for(int i = 1; i < rectList.size(); i++) {
	ptY = prevRect.y + recHeight + 1;
	rectList[i] = NbcRect(ptX, ptY, recWidth, recHeight, defaultColor);
	prevRect = rectList[i];
  }

  //--------------------------------------------------
  // set default selected rectangle
  if(MULTI_CROP_MODE)
	SelectRectangle(&rectList[0]);
  else
	SelectRectangle(&rectN);
}

void UpdateMainWindow() {
  //---------------------------------
  // first updates imgMarked

  // clean matrix
  imgMarked = imgOriginal.clone();

  // Update rectangles on image (re-draw)
  if(MULTI_CROP_MODE) {
	for(NbcRect r : rectList)
	  DrawRectangle(imgMarked, r);
  }
  else {
	if(showS)  DrawRectangle(imgMarked, rectS);
	if(showSW) DrawRectangle(imgMarked, rectSW);
	if(showN)  DrawRectangle(imgMarked, rectN);
	if(showNW) DrawRectangle(imgMarked, rectNW);
	if(showE)  DrawRectangle(imgMarked, rectE);
  }

  //------------------------------------------------
  // now obtain new view, cropping according to Zoom
  Mat imgView = Zoom::Apply(imgMarked);

  // acrescenta filename on upper left corner
  // Rect rect_fname = Rect(0, 0, 50, 20);
  // cv::rectangle(imgMarked, rect_fname, CV_RGB(127, 127, 127), CV_FILLED, 4, 0);
  cv::putText(imgView, cur_fname, Point(5,35-(5*Zoom::GetZoomState())), CV_FONT_VECTOR0, 0.35*abs(4-Zoom::GetZoomState()), CV_RGB(0, 0, 0));

  if(negativeFilt)
	cv::bitwise_not(imgView, imgView);

  // atualiza janela com imagem dos retangulos movidos
  imshow(MAIN_WINDOW_NAME, imgView);

  // updates preview image
  UpdatePreviewWindow();
}

bool ToggleRect() {
  if(MULTI_CROP_MODE) {
	// finds current selected rect index
	int r_index = 0;
	for(NbcRect &r : rectList) {
	  if(r == *selectedRect)
		break;
	  else
		r_index++;
	}
	if(selectedRect == &(rectList.back()))
	  r_index = 0;
	else
	  r_index++;

	SelectRectangle(&(rectList[r_index]));
  }
  
  return MULTI_CROP_MODE;
}

void SelectRectangle(NbcRect *r) {
  // set previous selected rectangle to default color
  if(selectedRect != NULL)
	selectedRect->color = defaultColor;

  // select new rectangle
  selectedRect = r;
  selectedRect->color = selectedColor;
}

void SelectRectangle(int x, int y){
  // set previous selected rectangle to default color
  selectedRect->color = defaultColor;

  if(MULTI_CROP_MODE){
	for(NbcRect &r : rectList) {
	  if(r.contains(Point(x, y))) {
		selectedRect = &r;
		break;
	  }
	}
  }
  else {
	if     (showS  && rectS .contains(Point(x, y))) selectedRect = &rectS;
	else if(showSW && rectSW.contains(Point(x, y))) selectedRect = &rectSW;
	else if(showN  && rectN .contains(Point(x, y))) selectedRect = &rectN;
	else if(showNW && rectNW.contains(Point(x, y))) selectedRect = &rectNW;
	else if(showE  && rectE .contains(Point(x, y))) selectedRect = &rectE;
  }

  // set new selected rectangle to selected color
  selectedRect->color = selectedColor;
}

bool IsValidPos(NbcRect *r, int movedX, int movedY) {
  // rectangle internal padding in order to surround crop area
  // movedX += 1;
  // movedY += 1;

  bool notValidX = r->mvStart.x + movedX + r->width  >= imgMarked.cols || r->mvStart.x + movedX < 0;
  bool notValidY = r->mvStart.y + movedY + r->height >= imgMarked.rows || r->mvStart.y + movedY < 0;

  if(notValidX || notValidY)
	  return false;

  return true;
}

void MoveRect(NbcRect *r, int movedX, int movedY) {
  if(IsValidPos(r, movedX, movedY)) {
	r->Move(movedX, movedY);
	r->ResetStart();
  }
}

void MouseMoveRect(NbcRect *r, Point mouseEnd) {
  // calcula amplitude do movimento
  int movedX = mouseEnd.x - mouseStart.x;
  int movedY = mouseEnd.y - mouseStart.y;

  // Atualiza posições dos retângulos
  if(IsValidPos(r, movedX, movedY))
	r->Move(movedX, movedY);
}

void SetRectPos(NbcRect *r, Point pos) {
  int newX = pos.x - r->width/2;
  int newY = pos.y - r->height/2;

  // check and fix X position
  if(newX < 0) r->x = 1;
  else if(newX + r->width >= imgMarked.cols) r->x = (imgMarked.cols-1) - r->width;
  else r->x = newX;

  // check and fix Y position
  if(newY < 0) r->y = 1;
  else if(newY + r->height >= imgMarked.rows) r->y = (imgMarked.rows-1) - r->height;
  else r->y = newY;

  r->ResetStart();
}

void ToggleRectOrientation(NbcRect *r) {
  if(r->width == recWidth) {
	r->width = recHeight;
	r->height = recWidth;
  }
  else { // r->widwth == recHeight
	r->width = recWidth;
	r->height = recHeight;
  }
}

void UpdatePreviewWindow() {
  double resizeFactor = PREVIEW_WIN_WIDTH/(double)recWidth; 
  Size newSize = Size();

  if(MULTI_CROP_MODE){
	// if even number of rects
	if((rectList.size() % 2 == 0)) {
	  // Mat's to concat, starts with the first element
	  Mat mRight, mLeft = FixMatOrientation(imgMarked(rectList[0]).clone());
	  int midIndex = rectList.size()/2;

	  // first half, top row
	  Mat topRow;
	  for(int i = 1; i < midIndex; i++) {
		mRight = FixMatOrientation(imgMarked(rectList[i]).clone());
		hconcat(mLeft, mRight, topRow);
		mLeft = topRow.clone();
	  }

	  // second half, bottom row
	  Mat bottomRow;
	  mLeft = FixMatOrientation(imgMarked(rectList[midIndex]).clone());
	  for(int i = midIndex + 1; i < rectList.size(); i++) {
		mRight = FixMatOrientation(imgMarked(rectList[i]).clone());
		hconcat(mLeft, mRight, bottomRow);
		mLeft = bottomRow.clone();
	  }

	  // concat top and bottom row
	  vconcat(topRow, bottomRow, previewImg);
	}
  }
  else {
	if(selectedRect->height > selectedRect->width
	   && cropDirection == hor)
	  previewImg = imgMarked(*selectedRect).t();
	else
  	  previewImg = imgMarked(*selectedRect);

	newSize.width = selectedRect->width  * resizeFactor;
	newSize.height = selectedRect->height  * resizeFactor;
  }
  
  // resizeWindow(DETAIL_WINDOW_NAME, newSize.width, newSize.height);
  // imshow(DETAIL_WINDOW_NAME, previewImg);
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

bool IsPortrait(Mat img) { return img.cols < img.rows; }

Mat FixMatOrientation(Mat rImg) {
  if(cropDirection == hor && IsPortrait(rImg))
	  return rImg.t();
  else
	return rImg;
}
