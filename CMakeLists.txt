cmake_minimum_required(VERSION 3.16)

option(ORB_SLAM3_BUILD_EXAMPLES "Build ORB_SLAM3 example executables" ON)
option(ORB_SLAM3_ENABLE_VIEWER "Build Pangolin viewer support" OFF)
option(ORB_SLAM3_USE_NATIVE_ARCH "Enable -march=native on release-like builds" ON)

set(ORB_SLAM3_VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "" FORCE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnableVcpkg.cmake")

project(ORB_SLAM3 VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules")

set(ORB_SLAM3_OPENCV_COMPONENTS
  core
  features2d
  imgproc
  calib3d
  highgui
  imgcodecs
  videoio
  aruco
)

find_package(OpenCV 4 QUIET COMPONENTS ${ORB_SLAM3_OPENCV_COMPONENTS})
if(NOT OpenCV_FOUND)
  find_package(OpenCV 3.0 QUIET COMPONENTS ${ORB_SLAM3_OPENCV_COMPONENTS})
endif()
if(NOT OpenCV_FOUND)
  message(FATAL_ERROR "OpenCV >= 3.0 with the aruco module is required")
endif()

find_package(Eigen3 3.1.0 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS serialization)
find_package(Threads REQUIRED)

if(ORB_SLAM3_ENABLE_VIEWER)
  find_package(Pangolin QUIET)
  if(NOT Pangolin_FOUND)
    message(
      FATAL_ERROR
      "ORB_SLAM3_ENABLE_VIEWER=ON requires a manual Pangolin installation. "
      "Pangolin is no longer provided by this repo's vcpkg manifest. "
      "Install Pangolin separately and make it discoverable via CMAKE_PREFIX_PATH or your system package paths."
    )
  endif()
endif()

if(ORB_SLAM3_USE_NATIVE_ARCH)
  add_compile_options(
    "$<$<CONFIG:Release>:-march=native>"
    "$<$<CONFIG:RelWithDebInfo>:-march=native>"
  )
endif()

add_compile_options(
  "$<$<COMPILE_LANGUAGE:C>:-Wall>"
  "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(Thirdparty/DBoW2)
add_subdirectory(Thirdparty/g2o)

set(ORB_SLAM3_PUBLIC_HEADERS
  include/Atlas.h
  include/Config.h
  include/Converter.h
  include/Frame.h
  include/FrameDrawer.h
  include/G2oTypes.h
  include/GeometricTools.h
  include/ImuTypes.h
  include/KeyFrame.h
  include/KeyFrameDatabase.h
  include/LocalMapping.h
  include/LoopClosing.h
  include/MLPnPsolver.h
  include/Map.h
  include/MapDrawer.h
  include/MapPoint.h
  include/ORBextractor.h
  include/ORBmatcher.h
  include/ORBVocabulary.h
  include/OptimizableTypes.h
  include/Optimizer.h
  include/SerializationUtils.h
  include/Settings.h
  include/Sim3Solver.h
  include/System.h
  include/Tracking.h
  include/TwoViewReconstruction.h
  include/Viewer.h
  include/CameraModels/GeometricCamera.h
  include/CameraModels/KannalaBrandt8.h
  include/CameraModels/Pinhole.h
)

set(ORB_SLAM3_SOURCES
  src/Atlas.cc
  src/Config.cc
  src/Converter.cc
  src/Frame.cc
  src/FrameDrawer.cc
  src/G2oTypes.cc
  src/GeometricTools.cc
  src/ImuTypes.cc
  src/KeyFrame.cc
  src/KeyFrameDatabase.cc
  src/LocalMapping.cc
  src/LoopClosing.cc
  src/MLPnPsolver.cpp
  src/Map.cc
  src/MapDrawer.cc
  src/MapPoint.cc
  src/ORBextractor.cc
  src/ORBmatcher.cc
  src/OptimizableTypes.cpp
  src/Optimizer.cc
  src/Settings.cc
  src/Sim3Solver.cc
  src/System.cc
  src/Tracking.cc
  src/TwoViewReconstruction.cc
  src/CameraModels/KannalaBrandt8.cpp
  src/CameraModels/Pinhole.cpp
)

if(ORB_SLAM3_ENABLE_VIEWER)
  list(APPEND ORB_SLAM3_SOURCES src/Viewer.cc)
endif()

add_library(ORB_SLAM3 SHARED ${ORB_SLAM3_PUBLIC_HEADERS} ${ORB_SLAM3_SOURCES})
add_library(ORB_SLAM3::ORB_SLAM3 ALIAS ORB_SLAM3)

target_compile_definitions(ORB_SLAM3 PRIVATE COMPILEDWITHC14)
if(ORB_SLAM3_ENABLE_VIEWER)
  target_compile_definitions(ORB_SLAM3 PUBLIC ORB_SLAM3_ENABLE_VIEWER)
endif()

target_include_directories(
  ORB_SLAM3
  PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/CameraModels"
    "${PROJECT_SOURCE_DIR}/Thirdparty/Sophus"
    ${OpenCV_INCLUDE_DIRS}
)

if(ORB_SLAM3_ENABLE_VIEWER)
  target_include_directories(
    ORB_SLAM3
    PRIVATE
    ${Pangolin_INCLUDE_DIRS}
  )
endif()

target_link_libraries(
  ORB_SLAM3
  PUBLIC
    Eigen3::Eigen
  PRIVATE
    DBoW2
    g2o
    ${OpenCV_LIBS}
    Boost::serialization
    OpenSSL::Crypto
    Threads::Threads
)

if(ORB_SLAM3_ENABLE_VIEWER)
  target_link_libraries(ORB_SLAM3 PRIVATE ${Pangolin_LIBRARIES})
endif()

set_target_properties(
  ORB_SLAM3
  PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
    OUTPUT_NAME "ORB_SLAM3"
)

if(ORB_SLAM3_BUILD_EXAMPLES)
  set(ORB_SLAM3_EXAMPLE_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/Examples/Monocular-Inertial")

  add_executable(
    mono_inertial_gopro_vi
    Examples/Monocular-Inertial/mono_inertial_gopro_vi.cc
  )
  set_target_properties(
    mono_inertial_gopro_vi
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${ORB_SLAM3_EXAMPLE_OUTPUT_DIR}"
  )
  target_link_libraries(mono_inertial_gopro_vi PRIVATE ORB_SLAM3::ORB_SLAM3)

  add_executable(
    mono_inertial_gopro_localize
    Examples/Monocular-Inertial/mono_inertial_gopro_localize.cc
  )
  set_target_properties(
    mono_inertial_gopro_localize
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${ORB_SLAM3_EXAMPLE_OUTPUT_DIR}"
  )
  target_link_libraries(mono_inertial_gopro_localize PRIVATE ORB_SLAM3::ORB_SLAM3)

  add_executable(
    gopro_slam
    Examples/Monocular-Inertial/gopro_slam.cc
  )
  set_target_properties(
    gopro_slam
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${ORB_SLAM3_EXAMPLE_OUTPUT_DIR}"
  )
  target_link_libraries(gopro_slam PRIVATE ORB_SLAM3::ORB_SLAM3)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
