cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0148)
  cmake_policy(SET CMP0148 NEW)
endif()

project(py_orb_slam3 VERSION 0.1.0 LANGUAGES C CXX)

option(PY_ORB_SLAM3_WITH_PANGOLIN "Enable Pangolin viewer support" OFF)

if(PY_ORB_SLAM3_WITH_PANGOLIN)
  message(FATAL_ERROR "Pangolin viewer builds are not supported in this repository yet. Configure with -DPY_ORB_SLAM3_WITH_PANGOLIN=OFF.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS aruco calib3d core features2d flann imgcodecs imgproc)
find_package(Eigen3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS serialization)
find_package(OpenSSL REQUIRED)

set(DBOW2_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(G2O_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(ORB_SLAM3/Thirdparty/DBoW2)
add_subdirectory(ORB_SLAM3/Thirdparty/g2o)

set_target_properties(DBoW2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(g2o PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ORB_SLAM3_CORE_SOURCES
  ORB_SLAM3/src/System.cc
  ORB_SLAM3/src/Tracking.cc
  ORB_SLAM3/src/LocalMapping.cc
  ORB_SLAM3/src/LoopClosing.cc
  ORB_SLAM3/src/ORBextractor.cc
  ORB_SLAM3/src/ORBmatcher.cc
  ORB_SLAM3/src/FrameDrawer.cc
  ORB_SLAM3/src/Converter.cc
  ORB_SLAM3/src/MapPoint.cc
  ORB_SLAM3/src/KeyFrame.cc
  ORB_SLAM3/src/Atlas.cc
  ORB_SLAM3/src/Map.cc
  ORB_SLAM3/src/MapDrawer.cc
  ORB_SLAM3/src/Optimizer.cc
  ORB_SLAM3/src/Frame.cc
  ORB_SLAM3/src/KeyFrameDatabase.cc
  ORB_SLAM3/src/Sim3Solver.cc
  ORB_SLAM3/src/Viewer.cc
  ORB_SLAM3/src/ImuTypes.cc
  ORB_SLAM3/src/G2oTypes.cc
  ORB_SLAM3/src/CameraModels/Pinhole.cpp
  ORB_SLAM3/src/CameraModels/KannalaBrandt8.cpp
  ORB_SLAM3/src/OptimizableTypes.cpp
  ORB_SLAM3/src/MLPnPsolver.cpp
  ORB_SLAM3/src/GeometricTools.cc
  ORB_SLAM3/src/TwoViewReconstruction.cc
  ORB_SLAM3/src/Config.cc
  ORB_SLAM3/src/Settings.cc
)

add_library(orbslam3_core STATIC ${ORB_SLAM3_CORE_SOURCES})
target_compile_definitions(orbslam3_core PUBLIC ORB_SLAM3_USE_PANGOLIN=0)
target_include_directories(
  orbslam3_core
  PUBLIC
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/ORB_SLAM3
    ${CMAKE_CURRENT_SOURCE_DIR}/ORB_SLAM3/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ORB_SLAM3/include/CameraModels
    ${CMAKE_CURRENT_SOURCE_DIR}/ORB_SLAM3/Thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/ORB_SLAM3/Thirdparty/DBoW2
    ${CMAKE_CURRENT_SOURCE_DIR}/ORB_SLAM3/Thirdparty/g2o
    ${CMAKE_CURRENT_SOURCE_DIR}/ORB_SLAM3/Thirdparty/Sophus
)
target_link_libraries(
  orbslam3_core
  PUBLIC
    Eigen3::Eigen
  PRIVATE
    DBoW2
    g2o
    Boost::serialization
    OpenSSL::Crypto
    ${OpenCV_LIBS}
)

pybind11_add_module(_pyorbslam3 MODULE src/py_orb_slam3/bindings.cpp)
target_compile_features(_pyorbslam3 PRIVATE cxx_std_17)
target_link_libraries(_pyorbslam3 PRIVATE orbslam3_core ${OpenCV_LIBS})
target_include_directories(_pyorbslam3 PRIVATE ${OpenCV_INCLUDE_DIRS})

install(TARGETS _pyorbslam3 LIBRARY DESTINATION py_orb_slam3)
install(FILES ORB_SLAM3/Vocabulary/ORBvoc.txt.tar.gz DESTINATION py_orb_slam3/data)
