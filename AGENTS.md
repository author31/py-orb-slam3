# Goal of this repository
Implement a python wrapper, py_orb_slam3, for ORB_SLAM3.

# Procedure
1. Submodule ORB_SLAM3 into this repository, under "ORB_SLAM3/" directory.
2. The original CMakeLists.txt from ORB_SLAM3 is migrated to the root of the repository.
2.1 Use vcpkg to install dependencies for ORB_SLAM3.
3. src/py_orb_slam3 includes the wrapping logics for ORB_SLAM3.
