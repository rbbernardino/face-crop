cmake_minimum_required(VERSION 2.8)

project( facecrop ) # PROJECT NAME

# set output dir of "make" (binary executable)
# set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build)

# set output dir of library files (.so??)
# set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_definitions(-std=c++14)

find_package( OpenCV REQUIRED )
FIND_PACKAGE( Boost COMPONENTS system REQUIRED )

set( HEADERS main.hpp NbcRect.hpp Zoom.hpp )
set( SOURCES main.cpp NbcRect.cpp Zoom.cpp )

include_directories("lib")

# set as static build, so the client doesn't need to have any lib installed
SET(BUILD_SHARED_LIBRARIES OFF)

add_executable( facecrop ${SOURCES} ${HEADERS} )
target_link_libraries(
  facecrop
  ${OpenCV_LIBS}
  ${Boost_SYSTEM_LIBRARY}
  stdc++fs
  )
