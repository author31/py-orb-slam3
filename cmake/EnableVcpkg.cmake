if(DEFINED ORB_SLAM3_ENABLE_VCPKG_INCLUDED)
  return()
endif()
set(ORB_SLAM3_ENABLE_VCPKG_INCLUDED TRUE)

option(ORB_SLAM3_USE_VCPKG "Resolve native dependencies with vcpkg when available" ON)
option(ORB_SLAM3_BOOTSTRAP_VCPKG "Bootstrap a repo-local copy of vcpkg when needed" ON)

if(NOT ORB_SLAM3_USE_VCPKG)
  return()
endif()

if(NOT DEFINED ORB_SLAM3_VCPKG_MANIFEST_DIR)
  set(
    ORB_SLAM3_VCPKG_MANIFEST_DIR
    "${CMAKE_CURRENT_LIST_DIR}/.."
    CACHE PATH
    "Directory containing the ORB_SLAM3 vcpkg manifest"
  )
endif()

if(NOT DEFINED ORB_SLAM3_VCPKG_ROOT)
  set(
    ORB_SLAM3_VCPKG_ROOT
    "${ORB_SLAM3_VCPKG_MANIFEST_DIR}/.vcpkg/vcpkg"
    CACHE PATH
    "Location of the repo-local vcpkg checkout"
  )
endif()

if((NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "") AND
   (NOT DEFINED ENV{CMAKE_TOOLCHAIN_FILE} OR "$ENV{CMAKE_TOOLCHAIN_FILE}" STREQUAL ""))
  set(_orb_slam3_vcpkg_candidates)
  set(_orb_slam3_repo_vcpkg_toolchain "${ORB_SLAM3_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

  if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    list(APPEND _orb_slam3_vcpkg_candidates "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  endif()

  if(EXISTS "${_orb_slam3_repo_vcpkg_toolchain}")
    list(APPEND _orb_slam3_vcpkg_candidates "${_orb_slam3_repo_vcpkg_toolchain}")
  elseif(ORB_SLAM3_BOOTSTRAP_VCPKG)
    find_program(ORB_SLAM3_GIT_EXECUTABLE NAMES git)
    if(NOT ORB_SLAM3_GIT_EXECUTABLE)
      message(FATAL_ERROR "git is required to bootstrap vcpkg. Install git or set CMAKE_TOOLCHAIN_FILE/VCPKG_ROOT explicitly.")
    endif()

    file(MAKE_DIRECTORY "${ORB_SLAM3_VCPKG_MANIFEST_DIR}/.vcpkg")

    if(EXISTS "${ORB_SLAM3_VCPKG_ROOT}/bootstrap-vcpkg.sh")
      message(STATUS "Bootstrapping repo-local vcpkg in ${ORB_SLAM3_VCPKG_ROOT}")
    elseif(NOT EXISTS "${ORB_SLAM3_VCPKG_ROOT}")
      message(STATUS "Cloning vcpkg into ${ORB_SLAM3_VCPKG_ROOT}")
      execute_process(
        COMMAND "${ORB_SLAM3_GIT_EXECUTABLE}" clone --filter=blob:none https://github.com/microsoft/vcpkg.git "${ORB_SLAM3_VCPKG_ROOT}"
        RESULT_VARIABLE _orb_slam3_vcpkg_clone_result
      )
      if(NOT _orb_slam3_vcpkg_clone_result EQUAL 0)
        message(FATAL_ERROR "Failed to clone vcpkg into ${ORB_SLAM3_VCPKG_ROOT}")
      endif()
    else()
      message(FATAL_ERROR "Found an incomplete vcpkg checkout at ${ORB_SLAM3_VCPKG_ROOT}. Remove it or set ORB_SLAM3_VCPKG_ROOT to a valid checkout.")
    endif()

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env VCPKG_DISABLE_METRICS=1 "${ORB_SLAM3_VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
      WORKING_DIRECTORY "${ORB_SLAM3_VCPKG_ROOT}"
      RESULT_VARIABLE _orb_slam3_vcpkg_bootstrap_result
    )
    if(NOT _orb_slam3_vcpkg_bootstrap_result EQUAL 0)
      message(FATAL_ERROR "Failed to bootstrap vcpkg in ${ORB_SLAM3_VCPKG_ROOT}")
    endif()

    if(EXISTS "${_orb_slam3_repo_vcpkg_toolchain}")
      list(APPEND _orb_slam3_vcpkg_candidates "${_orb_slam3_repo_vcpkg_toolchain}")
    endif()
  else()
    find_program(ORB_SLAM3_VCPKG_EXECUTABLE NAMES vcpkg)
    if(ORB_SLAM3_VCPKG_EXECUTABLE)
      get_filename_component(_orb_slam3_vcpkg_bin_dir "${ORB_SLAM3_VCPKG_EXECUTABLE}" DIRECTORY)
      list(APPEND _orb_slam3_vcpkg_candidates
        "${_orb_slam3_vcpkg_bin_dir}/scripts/buildsystems/vcpkg.cmake"
        "${_orb_slam3_vcpkg_bin_dir}/../scripts/buildsystems/vcpkg.cmake"
      )
    endif()
  endif()

  foreach(_orb_slam3_vcpkg_candidate IN LISTS _orb_slam3_vcpkg_candidates)
    get_filename_component(_orb_slam3_vcpkg_candidate_abs "${_orb_slam3_vcpkg_candidate}" ABSOLUTE)
    if(EXISTS "${_orb_slam3_vcpkg_candidate_abs}")
      set(
        CMAKE_TOOLCHAIN_FILE
        "${_orb_slam3_vcpkg_candidate_abs}"
        CACHE FILEPATH
        "vcpkg toolchain file for ORB_SLAM3"
        FORCE
      )
      break()
    endif()
  endforeach()
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
  message(STATUS "ORB_SLAM3_USE_VCPKG is ON, but no vcpkg toolchain was found. Falling back to system packages.")
  return()
endif()

if(NOT DEFINED VCPKG_MANIFEST_DIR)
  set(
    VCPKG_MANIFEST_DIR
    "${ORB_SLAM3_VCPKG_MANIFEST_DIR}"
    CACHE PATH
    "Directory containing vcpkg.json"
    FORCE
  )
endif()

if(NOT DEFINED VCPKG_MANIFEST_INSTALL)
  set(VCPKG_MANIFEST_INSTALL ON CACHE BOOL "Install vcpkg manifest dependencies during configure" FORCE)
endif()

if(NOT DEFINED VCPKG_INSTALLED_DIR)
  set(
    VCPKG_INSTALLED_DIR
    "${CMAKE_BINARY_DIR}/vcpkg_installed"
    CACHE PATH
    "Directory where vcpkg installs manifest dependencies"
    FORCE
  )
endif()

if(NOT DEFINED VCPKG_INSTALL_OPTIONS)
  set(
    VCPKG_INSTALL_OPTIONS
    "--downloads-root=${CMAKE_BINARY_DIR}/vcpkg-downloads"
    "--x-buildtrees-root=${CMAKE_BINARY_DIR}/vcpkg-buildtrees"
    "--x-packages-root=${CMAKE_BINARY_DIR}/vcpkg-packages"
    CACHE STRING
    "Additional arguments passed to vcpkg install"
    FORCE
  )
endif()

message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
