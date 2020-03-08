# README #

A tool for creating face detection ground-truth either from pre-cropped dataset or manually cropping.

### How do I get set up? ###

* **C++11**
* **boost 1.58** (on linux: `libboost-all-dev`)
* **OpenCV 3.2**

### How to Run ###

- `./facecrop [-f] [-d] <FILE_OR_DIRECTORY> -g <GT_FILE>`
	- `-f filename`, only one file will be processed
	- `-d directory`, **all** files in `directory` will be processed
    - `-g gt_file`, tsv file with `file_name`, `x`, `y`, `width`, `height`

### Crop Modes ###

* **Corner:** Mouse click will set the top-left corner and move the rectangle
* **Douple corner:** Mouse click will first define the top-left corner and then the bottom-right one
	* Rectangle will be created only after the second click

### General Remarks ###

- **After cropping**, hitting `RET`, the image will be moved to `done` and the cropped part to `cropped`
- **If skipped**, hitting `ESC`, the image will be cropped later, being moved to `later`
- **If no face** is present, hitting `SPACE` will move the image to `no-face`
- `Crop Preview` window shows a **crop preview**


### Mouse Commands ###

- [x] **Left click** and drag anywhere in the image to move the crop rectangle
- [x] **Right click** anywhere in the image to move the crop rectangle to the clicked position

### Keyboard Shortcuts ###

* **q** - quits the program
* **RET** - crop the image like shown in the `Crop Preview` window
* **ESC** - skips the image (move to `later`)
* **Arrows** - move the rectangle `moveUnit` pixels
* **1, 2, 3 ... 0** - changes the `moveUnit` to 1, 5, 15 ... 500, 1000 pixels per move
* **z** - alternates between `Normal Mode` and `Split Crop Mode`
* **SPC** - move image to `no-face`
* **i** - invert colors (only displayed image, not the cropped result)
