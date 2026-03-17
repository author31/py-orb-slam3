cmake_minimum_required(VERSION 3.18)

set(ORB_SLAM3_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ORB_SLAM3_ENABLE_VIEWER OFF CACHE BOOL "" FORCE)
set(ORB_SLAM3_VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "" FORCE)
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EnableVcpkg.cmake")

project(pyorbslam3 LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS
  core
  features2d
  imgproc
  calib3d
  highgui
  imgcodecs
  videoio
  aruco
  objdetect
)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/orb_slam3")

pybind11_add_module(_pyorbslam3 MODULE src/bindings.cpp)

target_link_libraries(_pyorbslam3 PRIVATE ORB_SLAM3::ORB_SLAM3 ${OpenCV_LIBS})

target_compile_features(_pyorbslam3 PRIVATE cxx_std_17)

set_target_properties(
  _pyorbslam3
  ORB_SLAM3
  DBoW2
  g2o
  PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)

install(
  TARGETS
    _pyorbslam3
    ORB_SLAM3
    DBoW2
    g2o
  LIBRARY DESTINATION pyorbslam3
)
