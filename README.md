# py_orb_slam3

Headless Python bindings for ORB_SLAM3, built from the repository root with `uv` and `vcpkg`.

## Requirements

- Linux x86_64
- `uv`
- `vcpkg`
- `CMAKE_TOOLCHAIN_FILE` pointing at `vcpkg/scripts/buildsystems/vcpkg.cmake`

## Development workflow

```bash
export VCPKG_ROOT="$(dirname "$(dirname "$(command -v vcpkg)")")"
export CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

uv sync --group dev
uv run pytest
```

## Notes

- The supported build is headless-only. Pangolin viewer support is intentionally disabled.
- `ORB_SLAM3/build.sh` is deprecated and must not be used.
- Use `py_orb_slam3.get_vocabulary_path()` to resolve the packaged ORB vocabulary.
