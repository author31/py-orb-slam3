from __future__ import annotations

import os
import tarfile
from importlib import resources
from pathlib import Path


def _cache_dir() -> Path:
    override = os.environ.get("PY_ORB_SLAM3_CACHE_DIR")
    if override:
        return Path(override)
    return Path.home() / ".cache" / "py_orb_slam3"


def get_orb_slam3_submodule() -> Path:
    submodule_path = Path("ORB_SLAM3")
    if submodule_path.exists():
        return submodule_path
    raise FileNotFoundError("ORB_SLAM3 submodule not found")

def get_vocabulary_path() -> str:
    cache_dir = _cache_dir()
    cache_dir.mkdir(parents=True, exist_ok=True)
    vocabulary_path = cache_dir / "ORBvoc.txt"
    if vocabulary_path.exists():
        return str(vocabulary_path)

    orb_slam3_path = get_orb_slam3_submodule()
    archive_resource = orb_slam3_path / "Vocabulary" / "ORBvoc.txt.tar.gz"
    with resources.as_file(archive_resource) as archive_path:
        with tarfile.open(archive_path, "r:gz") as tar:
            tar.extractall(path=cache_dir)

    if not vocabulary_path.exists():
        raise FileNotFoundError(f"failed to extract ORB vocabulary from {archive_resource}")

    return str(vocabulary_path)
