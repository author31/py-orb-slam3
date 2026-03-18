# Build And Headless Porting Notes

Last updated: 2026-03-18

## Purpose

This document explains how the current `py_orb_slam3` build works, how `vcpkg`, `CMakeLists.txt`, `uv`, and `scikit-build-core` interact, which build failures were resolved during bring-up, and what decisions were made to keep the wrapper headless.

The goal of this repository is not to build the original interactive ORB-SLAM3 application. The goal is to build a Python package that exposes ORB-SLAM3 functionality without requiring Pangolin or GUI runtime support.

## Current Build Stack

### 1. `uv`

`uv` is the top-level developer command used in this repository.

`uv sync` does two jobs:

1. It creates or updates the Python environment.
2. It installs this package in editable mode through the PEP 517 build backend declared in `pyproject.toml`.

In practice, `uv sync` is the command that triggers the whole native build.

### 2. `scikit-build-core`

The build backend in [`pyproject.toml`](/home/hcis-s17/author_workdir/py_orb_slam3/pyproject.toml) is:

```toml
build-backend = "scikit_build_core.build"
```

`scikit-build-core` bridges Python packaging and CMake. It:

1. Creates an isolated build environment.
2. Installs Python-side build requirements such as `pybind11`.
3. Runs CMake configure and build steps.
4. Installs the produced extension module into the editable environment.

Important implication:

- `pybind11` should come from the Python build environment created by `scikit-build-core`, not from `vcpkg`.

### 3. `CMakeLists.txt`

[`CMakeLists.txt`](/home/hcis-s17/author_workdir/py_orb_slam3/CMakeLists.txt) is the native build entrypoint. It is responsible for:

1. Declaring the project and compiler settings.
2. Finding Python, `pybind11`, OpenCV, Eigen, Boost, and OpenSSL.
3. Building ORB-SLAM3 core code as a static library named `orbslam3_core`.
4. Building the Python extension module `_pyorbslam3`.

Current high-level structure:

- `orbslam3_core`: static library containing ORB-SLAM3 sources plus `DBoW2` and `g2o`.
- `_pyorbslam3`: pybind11 extension that links against `orbslam3_core`.

Headless behavior is enforced with:

- `PY_ORB_SLAM3_WITH_PANGOLIN=OFF`
- `ORB_SLAM3_USE_PANGOLIN=0`
- a hard CMake error if Pangolin support is requested

### 4. `vcpkg`

[`vcpkg.json`](/home/hcis-s17/author_workdir/py_orb_slam3/vcpkg.json) is the dependency manifest for C++ dependencies. `vcpkg` is invoked during the CMake configure step through:

```toml
cmake.args = [
    "-DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake",
    "-DVCPKG_INSTALLED_DIR=./vcpkg_installed",
    "-DBUILD_PYTHON_EXTENSION=ON"
]
```

This means CMake dependency resolution happens through the `vcpkg` toolchain first, and installed artifacts are placed in `./vcpkg_installed`.

Current manifest dependencies:

- `boost-serialization`
- `eigen3`
- `opencv4` with explicit non-default features
- `openssl`

## End-To-End Build Flow

The current flow of `uv sync` is:

1. `uv` resolves Python dependencies.
2. `uv` asks `scikit-build-core` to build `py_orb_slam3`.
3. `scikit-build-core` creates an isolated build environment and installs Python build requirements.
4. `scikit-build-core` runs CMake configure.
5. CMake enters the `vcpkg` toolchain and installs or validates C++ dependencies from `vcpkg.json`.
6. CMake configures the native build:
   - `find_package(Python ...)`
   - `find_package(pybind11 ...)`
   - `find_package(OpenCV ...)`
   - `find_package(Eigen3 ...)`
   - `find_package(Boost ...)`
   - `find_package(OpenSSL ...)`
7. Ninja builds:
   - `DBoW2`
   - `g2o`
   - `orbslam3_core`
   - `_pyorbslam3`
8. `scikit-build-core` installs the resulting extension into the editable Python environment.

## Current Dependency Decisions

### Why OpenCV uses explicit features

The repository now uses:

```json
{
  "name": "opencv4",
  "default-features": false,
  "features": ["calib3d", "contrib", "jpeg", "png", "tiff", "webp"]
}
```

Reason:

- The default `opencv4` feature set on Linux pulls GUI-related features such as `gtk` and `highgui`.
- Those bring in a large transitive dependency tree that is unnecessary for a headless Python wrapper.
- ORB-SLAM3 still needs `aruco`, which is provided through OpenCV contrib in this configuration.

### Why `pybind11` is not in `vcpkg.json`

`pybind11` is intentionally provided by the Python build backend, not by `vcpkg`.

Reason:

- `scikit-build-core` already installs `pybind11` in its isolated environment.
- Pulling `pybind11` through `vcpkg` caused `vcpkg` to build its own `python3` port.
- That, in turn, required additional system autotools that are unrelated to building this package.

### Why Pangolin is disabled at the CMake level

This repository is explicitly headless. Pangolin is not treated as optional runtime functionality; it is treated as unsupported build input for this packaging target.

Reason:

- The package is meant to expose ORB-SLAM3 tracking and mapping through Python.
- GUI viewer dependencies complicate packaging, CI, and deployment.
- The build should succeed on environments without desktop stacks.

## Resolved Issues

### Issue 1: `opencv4` default features pulled GTK and `gettext`

Symptom:

- `vcpkg install` tried to build `gtk3`, `gettext`, and related packages.
- Build failed with missing `bison`.

Root cause:

- The manifest used plain `opencv4`, which enables GUI-oriented default features on Linux.

Resolution:

- Changed `opencv4` to `default-features: false`.
- Selected only the OpenCV features actually needed by this repository.

Short explanation:

- The fix removed unnecessary GUI dependencies from the C++ dependency graph.

### Issue 2: `vcpkg` tried to build its own `python3`

Symptom:

- `vcpkg install` attempted to build `python3`.
- Build failed with missing `autoconf`, `autoconf-archive`, `automake`, and `libtool`.

Root cause:

- `pybind11` was listed in `vcpkg.json`.
- The `vcpkg` `pybind11` port pulls in a host Python toolchain.

Resolution:

- Removed `pybind11` from `vcpkg.json`.
- Kept `find_package(pybind11 CONFIG REQUIRED)` in CMake so it resolves from the `scikit-build-core` environment.

Short explanation:

- Python build requirements should stay in the Python packaging layer, not the C++ package manager layer.

### Issue 3: `CMAKE_C_COMPILER not set` / `CMAKE_CXX_COMPILER not set`

Symptom:

- CMake reported compiler variables not set after `EnableLanguage`.

Root cause:

- This was a secondary error after `vcpkg install` had already failed.

Resolution:

- No direct compiler fix was needed.
- Fixing the upstream `vcpkg` errors removed this symptom.

Short explanation:

- The compiler messages were fallout, not the primary problem.

### Issue 4: OpenCV 4.12 `aruco` API return type mismatch

Symptom:

- Compilation failed because `cv::aruco::getPredefinedDictionary(...)` returned `cv::aruco::Dictionary`, while repository code expected `cv::Ptr<cv::aruco::Dictionary>`.

Root cause:

- The ORB-SLAM3-based code was written against an older `aruco` API.

Resolution:

- Wrapped default dictionary creation with `cv::makePtr<cv::aruco::Dictionary>(...)` in:
  - [`ORB_SLAM3/include/Frame.h`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/include/Frame.h)
  - [`ORB_SLAM3/include/System.h`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/include/System.h)

Short explanation:

- The code kept pointer-based internal plumbing but adapted the new OpenCV return type at the boundary.

### Issue 5: `cv::aruco::DetectorParameters::create()` no longer exists

Symptom:

- Compilation failed in [`ORB_SLAM3/src/Frame.cc`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/src/Frame.cc).

Root cause:

- Newer OpenCV exposes `DetectorParameters` as a normal constructible type rather than the old `create()` API.

Resolution:

- Replaced:

```cpp
cv::aruco::DetectorParameters::create()
```

with:

```cpp
cv::makePtr<cv::aruco::DetectorParameters>()
```

Short explanation:

- This is a straightforward compatibility update for newer OpenCV.

### Issue 6: `bool` counter increment rejected by C++17

Symptom:

- Compilation failed in [`ORB_SLAM3/src/LoopClosing.cc`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/src/LoopClosing.cc) because `mnFullBAIdx++` was applied to a `bool`.

Root cause:

- [`ORB_SLAM3/include/LoopClosing.h`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/include/LoopClosing.h) declared `mnFullBAIdx` as `bool`, but the code uses it as a generation counter.

Resolution:

- Changed `mnFullBAIdx` from `bool` to `int`.

Short explanation:

- The declaration was inconsistent with actual usage.

### Issue 7: Unused HighGUI headers in headless code path

Symptom:

- Not always a hard failure, but these includes implied GUI coupling:
  - [`ORB_SLAM3/src/ORBextractor.cc`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/src/ORBextractor.cc)
  - [`ORB_SLAM3/src/FrameDrawer.cc`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/src/FrameDrawer.cc)

Root cause:

- Legacy includes remained even though those translation units do not require HighGUI APIs.

Resolution:

- Removed the unused `opencv2/highgui/highgui.hpp` includes.

Short explanation:

- This reduces accidental GUI coupling and better matches the headless design.

### Issue 8: Outdated `scikit-build-core` verbosity key

Symptom:

- Warning:

```text
Use build.verbose instead of cmake.verbose for scikit-build-core >= 0.10
```

Resolution:

- Changed `cmake.verbose = true` to `build.verbose = true` in [`pyproject.toml`](/home/hcis-s17/author_workdir/py_orb_slam3/pyproject.toml).

Short explanation:

- This aligns configuration with the current backend version.

## Headless Porting Process

### Step 1: Make headless support explicit

The repository does not quietly omit viewer code. It declares the headless build mode explicitly and rejects Pangolin-enabled builds.

Current mechanism:

- CMake option `PY_ORB_SLAM3_WITH_PANGOLIN`
- hard failure if it is set to `ON`
- compile definition `ORB_SLAM3_USE_PANGOLIN=0`

This is preferable to implicit behavior because it makes build intent obvious.

### Step 2: Keep viewer-facing classes linkable

ORB-SLAM3 still expects viewer-related classes and call sites to exist. Removing them entirely would be a larger invasive fork.

Instead, the repository keeps a headless implementation of [`ORB_SLAM3/src/Viewer.cc`](/home/hcis-s17/author_workdir/py_orb_slam3/ORB_SLAM3/src/Viewer.cc) that:

- preserves the class interface
- loads viewer-related settings where harmless
- performs no GUI work
- immediately reports finished/stopped states

Decision:

- Preserve interface compatibility.
- Eliminate runtime GUI behavior.

### Step 3: Minimize transitive GUI dependencies

Headless porting is not only about Pangolin. It also requires removing incidental GUI pulls from OpenCV and old includes.

Actions taken:

- removed OpenCV default GUI features in `vcpkg`
- removed unused HighGUI includes
- kept only the OpenCV modules needed by the package

Decision:

- Headless should be reflected both in source code and in the dependency graph.

### Step 4: Keep the Python package focused

The package exports:

- `System`
- `Sensor`
- `TrackingResult`
- `ImuPoint`
- vocabulary path helpers

from [`src/py_orb_slam3/__init__.py`](/home/hcis-s17/author_workdir/py_orb_slam3/src/py_orb_slam3/__init__.py).

The Python packaging target is not intended to ship the original ORB-SLAM3 examples or viewer applications.

Decision:

- Build only what is needed for the Python wrapper.

## Verification Performed

The current configuration was verified with:

```bash
uv sync
```

and direct extension import:

```bash
.venv/bin/python -c "from py_orb_slam3 import _pyorbslam3; print(_pyorbslam3.__file__)"
```

Both succeeded.

## Practical Guidance For Future Changes

If a future change touches build configuration:

1. Keep Python build requirements in `pyproject.toml`.
2. Keep native C/C++ libraries in `vcpkg.json`.
3. Treat `vcpkg` default features with suspicion, especially for OpenCV.
4. If `uv sync` fails in CMake with compiler-related messages, inspect the `vcpkg-manifest-install.log` first.
5. For ORB-SLAM3 source changes, assume compatibility issues may come from newer C++ standards and newer OpenCV APIs.

If a future change touches headless behavior:

1. Do not reintroduce Pangolin as an implicit dependency.
2. Do not add HighGUI includes unless they are strictly required.
3. Prefer interface-preserving no-op implementations over large invasive deletions when adapting upstream ORB-SLAM3 code.

