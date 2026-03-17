# py_orb_slam3 Architecture

`pyorbslam3` is a thin Python packaging and binding layer on top of the native `ORB_SLAM3` C++ library. The build starts in `pyproject.toml`, enters the Python-specific CMake entry point in `python/CMakeLists.txt`, builds the native shared library from the repository root `CMakeLists.txt`, and exposes a small Python API from `python/src/bindings.cpp`.

## 1. Headless Exposure

The Python package is intentionally headless. In `python/CMakeLists.txt`, the build forces `ORB_SLAM3_BUILD_EXAMPLES=OFF` and `ORB_SLAM3_ENABLE_VIEWER=OFF`, so the Python artifact never depends on Pangolin or the viewer executable path. The root `CMakeLists.txt` only compiles `Viewer.cc` and links Pangolin when viewer support is enabled, so the same native codebase can support both GUI and headless builds.

At runtime, the binding layer constructs `ORB_SLAM3::System` with `bUseViewer=false`. The Python-facing system therefore exposes tracking, localization, map lifecycle, and trajectory export without any viewer thread or GUI state. This makes the package suitable for servers, notebooks, batch jobs, and containerized environments.

## 2. What Is Exposed via pybind11

The pybind11 surface is deliberately small and high-level:

- `Sensor`: enum mapping the native ORB-SLAM3 sensor modes.
- `ImuPoint`: simple Python data carrier for accelerometer, gyroscope, and timestamp input.
- `TrackingResult`: structured return object containing success flag, `4x4` pose matrix, tracking state, tracked keypoints, and keypoint count.
- `System`: high-level wrapper around `ORB_SLAM3::System`.

The `System` wrapper exposes:

- construction from vocabulary path, settings path, sensor mode, and optional atlas load/save paths
- `track_monocular`, `track_stereo`, `track_rgbd`, and `localize_monocular`
- lifecycle/state operations such as `shutdown`, `reset`, `reset_active_map`, localization-mode toggles, and tracking-state queries
- trajectory export helpers for CSV, TUM, EuRoC, and KITTI formats

Internally, `python/src/bindings.cpp` validates NumPy inputs, wraps them as `cv::Mat`, converts Python IMU samples into native `ORB_SLAM3::IMU::Point`, releases the GIL around long-running native calls, and converts results back into Python-friendly Eigen/NumPy structures. Low-level internals such as keyframes, maps, optimizers, and viewer objects are not directly exposed.

## 3. vcpkg Utilization

Native dependencies are resolved with manifest-based vcpkg. The manifest in `vcpkg.json` declares the required third-party stack:

- `boost-serialization`
- `eigen3`
- `openssl`
- `opencv4`

Pangolin is intentionally not part of the manifest. Viewer-enabled native builds remain supported, but Pangolin must be installed manually outside vcpkg.

The shared helper `cmake/EnableVcpkg.cmake` is included by both the root CMake project and the Python CMake project. It:

- detects an existing vcpkg toolchain or bootstraps a repo-local copy under `.vcpkg/vcpkg`
- points CMake at the repository manifest
- enables manifest install automatically during configure
- places installed packages, downloads, and build artifacts inside the build tree for isolation and reproducibility

In practice, `uv sync` triggers `scikit-build-core`, which runs CMake, which invokes vcpkg manifest install, which resolves and builds the native dependency graph before compiling the ORB-SLAM3 shared library and the pybind11 extension.

## Packaging Layout

The Python build installs `_pyorbslam3` together with `libORB_SLAM3.so`, `libDBoW2.so`, and `libg2o.so` into the package directory and sets `$ORIGIN`-relative RPATH. This keeps the editable and installed package self-contained and avoids requiring `LD_LIBRARY_PATH` for normal imports.
