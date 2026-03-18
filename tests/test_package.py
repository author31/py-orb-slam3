from pathlib import Path

import pytest

import py_orb_slam3


def test_public_api_surface() -> None:
    assert py_orb_slam3.System is not None
    assert py_orb_slam3.Sensor is not None
    assert py_orb_slam3.ImuPoint is not None
    assert py_orb_slam3.TrackingResult is not None
    assert callable(py_orb_slam3.get_vocabulary_path)


def test_get_vocabulary_path_extracts_archive() -> None:
    vocabulary_path = Path(py_orb_slam3.get_vocabulary_path())
    assert vocabulary_path.is_file()
    assert vocabulary_path.name == "ORBvoc.txt"
    assert vocabulary_path.stat().st_size > 0


def test_system_constructor_raises_python_exception_for_missing_inputs() -> None:
    with pytest.raises(Exception):
        py_orb_slam3.System(
            "missing-vocabulary",
            "missing-settings",
            py_orb_slam3.Sensor.MONOCULAR,
        )


def test_system_constructor_raises_for_missing_settings_with_valid_vocab() -> None:
    with pytest.raises(Exception):
        py_orb_slam3.System(
            py_orb_slam3.get_vocabulary_path(),
            "missing-settings",
            py_orb_slam3.Sensor.MONOCULAR,
        )
