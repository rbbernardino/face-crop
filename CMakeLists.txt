cmake_minimum_required(VERSION 2.8)
project( facecrop )
find_package( OpenCV REQUIRED )
FIND_PACKAGE( Boost COMPONENTS filesystem system REQUIRED )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# C++14: set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")

set(HEADERS NbcRect.h Zoom.hpp main.h)
set(SOURCES main.cpp NbcRect.cpp Zoom.cpp)

add_executable( CropNabuco ${SOURCES} ${HEADERS} )
target_link_libraries( facecrop
  ${OpenCV_LIBS}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  )

# to add a library / header files for several targets
# set(HEADER_FILES ${YOUR_DIRECTORY}/file1.h ${YOUR_DIRECTORY}/file2.h)
# add_library(mylib libsrc.cpp ${HEADER_FILES})
# add_executable(myexec execfile.cpp ${HEADER_FILES})
