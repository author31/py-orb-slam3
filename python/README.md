# pyorbslam3

`pyorbslam3` provides Python bindings for the headless ORB_SLAM3 library using `pybind11` and `scikit-build-core`.

Install from the repository root with:

```bash
uv sync
```

Or install the package directly with:

```bash
pip install .
```

The build prefers a repo-local `vcpkg` checkout under `.vcpkg/` and bootstraps it automatically when needed, so native C++ dependencies can be fetched into the build directory without separate OS-level package installs. If you already manage `vcpkg` yourself, `VCPKG_ROOT` or `CMAKE_TOOLCHAIN_FILE` still overrides that default.

The package expects ORB vocabulary and settings files to be supplied at runtime.
