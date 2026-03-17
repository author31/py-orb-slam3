#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <opencv2/core.hpp>
#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "ImuTypes.h"
#include "System.h"
#include "Tracking.h"

namespace py = pybind11;

namespace {

struct ImuPoint {
    std::array<float, 3> accel;
    std::array<float, 3> gyro;
    double timestamp;

    ORB_SLAM3::IMU::Point ToNative() const
    {
        return ORB_SLAM3::IMU::Point(
            accel[0],
            accel[1],
            accel[2],
            gyro[0],
            gyro[1],
            gyro[2],
            timestamp
        );
    }
};

struct TrackingResult {
    bool success;
    Eigen::Matrix4f pose;
    int tracking_state;
    Eigen::MatrixXf keypoints;
    std::size_t tracked_keypoint_count;
};

bool IsContiguous(const py::buffer_info& info)
{
    ssize_t expected = static_cast<ssize_t>(info.itemsize);
    for (ssize_t axis = info.ndim - 1; axis >= 0; --axis) {
        if (info.strides[axis] != expected) {
            return false;
        }
        expected *= info.shape[axis];
    }
    return true;
}

cv::Mat ImageArrayToMat(const py::array& array, const char* arg_name)
{
    py::buffer_info info = array.request();
    if (!IsContiguous(info)) {
        throw py::value_error(std::string(arg_name) + " must be C-contiguous");
    }

    if (info.ndim == 2) {
        if (!py::isinstance<py::array_t<std::uint8_t>>(array)) {
            throw py::value_error(std::string(arg_name) + " must have dtype uint8");
        }
        return cv::Mat(
            static_cast<int>(info.shape[0]),
            static_cast<int>(info.shape[1]),
            CV_8UC1,
            info.ptr
        );
    }

    if (info.ndim == 3 && info.shape[2] == 3) {
        if (!py::isinstance<py::array_t<std::uint8_t>>(array)) {
            throw py::value_error(std::string(arg_name) + " must have dtype uint8");
        }
        return cv::Mat(
            static_cast<int>(info.shape[0]),
            static_cast<int>(info.shape[1]),
            CV_8UC3,
            info.ptr
        );
    }

    throw py::value_error(std::string(arg_name) + " must have shape (H, W) or (H, W, 3)");
}

cv::Mat DepthArrayToMat(const py::array& array, const char* arg_name)
{
    py::buffer_info info = array.request();
    if (!IsContiguous(info)) {
        throw py::value_error(std::string(arg_name) + " must be C-contiguous");
    }
    if (info.ndim != 2) {
        throw py::value_error(std::string(arg_name) + " must have shape (H, W)");
    }
    if (!py::isinstance<py::array_t<float>>(array)) {
        throw py::value_error(std::string(arg_name) + " must have dtype float32");
    }

    return cv::Mat(
        static_cast<int>(info.shape[0]),
        static_cast<int>(info.shape[1]),
        CV_32FC1,
        info.ptr
    );
}

Eigen::Matrix4f PoseToMatrix(const Sophus::SE3f& pose)
{
    return pose.matrix();
}

Eigen::MatrixXf KeyPointsToMatrix(const std::vector<cv::KeyPoint>& keypoints)
{
    Eigen::MatrixXf output(static_cast<Eigen::Index>(keypoints.size()), 2);
    for (Eigen::Index i = 0; i < output.rows(); ++i) {
        output(i, 0) = keypoints[static_cast<std::size_t>(i)].pt.x;
        output(i, 1) = keypoints[static_cast<std::size_t>(i)].pt.y;
    }
    return output;
}

bool TrackingSucceeded(int tracking_state)
{
    return tracking_state == ORB_SLAM3::Tracking::OK ||
           tracking_state == ORB_SLAM3::Tracking::OK_KLT;
}

std::vector<ORB_SLAM3::IMU::Point> ToNativeImu(const std::vector<ImuPoint>& points)
{
    std::vector<ORB_SLAM3::IMU::Point> output;
    output.reserve(points.size());
    for (const ImuPoint& point : points) {
        output.push_back(point.ToNative());
    }
    return output;
}

class SystemWrapper {
public:
    SystemWrapper(
        const std::string& vocabulary_path,
        const std::string& settings_path,
        ORB_SLAM3::System::eSensor sensor,
        const std::string& load_atlas_path,
        const std::string& save_atlas_path
    )
    {
        py::gil_scoped_release release;
        system_ = std::make_unique<ORB_SLAM3::System>(
            vocabulary_path,
            settings_path,
            sensor,
            false,
            load_atlas_path,
            save_atlas_path
        );
    }

    ~SystemWrapper()
    {
        if (system_ && !system_->isShutDown()) {
            try {
                system_->Shutdown();
            } catch (...) {
            }
        }
    }

    TrackingResult TrackMonocular(
        const py::array& image,
        double timestamp,
        const std::vector<ImuPoint>& imu_points,
        const std::string& filename
    )
    {
        cv::Mat native_image = ImageArrayToMat(image, "image");
        std::vector<ORB_SLAM3::IMU::Point> native_imu = ToNativeImu(imu_points);

        Sophus::SE3f pose;
        {
            py::gil_scoped_release release;
            pose = system_->TrackMonocular(native_image, timestamp, native_imu, filename);
        }

        return BuildTrackingResult(pose, TrackingSucceeded(system_->GetTrackingState()));
    }

    TrackingResult TrackStereo(
        const py::array& left_image,
        const py::array& right_image,
        double timestamp,
        const std::vector<ImuPoint>& imu_points,
        const std::string& filename
    )
    {
        cv::Mat native_left = ImageArrayToMat(left_image, "left_image");
        cv::Mat native_right = ImageArrayToMat(right_image, "right_image");
        std::vector<ORB_SLAM3::IMU::Point> native_imu = ToNativeImu(imu_points);

        Sophus::SE3f pose;
        {
            py::gil_scoped_release release;
            pose = system_->TrackStereo(native_left, native_right, timestamp, native_imu, filename);
        }

        return BuildTrackingResult(pose, TrackingSucceeded(system_->GetTrackingState()));
    }

    TrackingResult TrackRgbd(
        const py::array& image,
        const py::array& depth,
        double timestamp,
        const std::vector<ImuPoint>& imu_points,
        const std::string& filename
    )
    {
        cv::Mat native_image = ImageArrayToMat(image, "image");
        cv::Mat native_depth = DepthArrayToMat(depth, "depth");
        std::vector<ORB_SLAM3::IMU::Point> native_imu = ToNativeImu(imu_points);

        Sophus::SE3f pose;
        {
            py::gil_scoped_release release;
            pose = system_->TrackRGBD(native_image, native_depth, timestamp, native_imu, filename);
        }

        return BuildTrackingResult(pose, TrackingSucceeded(system_->GetTrackingState()));
    }

    TrackingResult LocalizeMonocular(
        const py::array& image,
        double timestamp,
        const std::vector<ImuPoint>& imu_points,
        const std::string& filename
    )
    {
        cv::Mat native_image = ImageArrayToMat(image, "image");
        std::vector<ORB_SLAM3::IMU::Point> native_imu = ToNativeImu(imu_points);

        std::pair<Sophus::SE3f, bool> result;
        {
            py::gil_scoped_release release;
            result = system_->LocalizeMonocular(native_image, timestamp, native_imu, filename);
        }

        return BuildTrackingResult(result.first, result.second);
    }

    void Shutdown()
    {
        py::gil_scoped_release release;
        system_->Shutdown();
    }

    void Reset() { system_->Reset(); }
    void ResetActiveMap() { system_->ResetActiveMap(); }
    void ActivateLocalizationMode() { system_->ActivateLocalizationMode(); }
    void DeactivateLocalizationMode() { system_->DeactivateLocalizationMode(); }
    bool MapChanged() { return system_->MapChanged(); }
    bool IsShutdown() { return system_->isShutDown(); }
    bool IsLost() { return system_->isLost(); }
    bool IsFinished() { return system_->isFinished(); }
    double GetTimeFromImuInit() { return system_->GetTimeFromIMUInit(); }
    float GetImageScale() { return system_->GetImageScale(); }
    bool IsLoadingMap() { return system_->isLoadingMap(); }
    int GetTrackingState() { return system_->GetTrackingState(); }

    void SaveTrajectoryCsv(const std::string& filename) { system_->SaveTrajectoryCSV(filename); }
    void SaveTrajectoryTum(const std::string& filename) { system_->SaveTrajectoryTUM(filename); }
    void SaveKeyFrameTrajectoryTum(const std::string& filename) { system_->SaveKeyFrameTrajectoryTUM(filename); }
    void SaveTrajectoryEuroc(const std::string& filename) { system_->SaveTrajectoryEuRoC(filename); }
    void SaveKeyFrameTrajectoryEuroc(const std::string& filename) { system_->SaveKeyFrameTrajectoryEuRoC(filename); }
    void SaveTrajectoryKitti(const std::string& filename) { system_->SaveTrajectoryKITTI(filename); }

private:
    TrackingResult BuildTrackingResult(const Sophus::SE3f& pose, bool success)
    {
        const std::vector<cv::KeyPoint> keypoints = system_->GetTrackedKeyPointsUn();
        return TrackingResult{
            success,
            PoseToMatrix(pose),
            system_->GetTrackingState(),
            KeyPointsToMatrix(keypoints),
            keypoints.size(),
        };
    }

    std::unique_ptr<ORB_SLAM3::System> system_;
};

}  // namespace

PYBIND11_MODULE(_pyorbslam3, m)
{
    m.doc() = "Python bindings for ORB_SLAM3";

    py::enum_<ORB_SLAM3::System::eSensor>(m, "Sensor")
        .value("MONOCULAR", ORB_SLAM3::System::MONOCULAR)
        .value("STEREO", ORB_SLAM3::System::STEREO)
        .value("RGBD", ORB_SLAM3::System::RGBD)
        .value("IMU_MONOCULAR", ORB_SLAM3::System::IMU_MONOCULAR)
        .value("IMU_STEREO", ORB_SLAM3::System::IMU_STEREO)
        .value("IMU_RGBD", ORB_SLAM3::System::IMU_RGBD)
        .export_values();

    py::class_<ImuPoint>(m, "ImuPoint")
        .def(
            py::init<std::array<float, 3>, std::array<float, 3>, double>(),
            py::arg("accel"),
            py::arg("gyro"),
            py::arg("timestamp")
        )
        .def_readwrite("accel", &ImuPoint::accel)
        .def_readwrite("gyro", &ImuPoint::gyro)
        .def_readwrite("timestamp", &ImuPoint::timestamp);

    py::class_<TrackingResult>(m, "TrackingResult")
        .def_property_readonly("success", [](const TrackingResult& result) { return result.success; })
        .def_property_readonly("pose", [](const TrackingResult& result) { return result.pose; })
        .def_property_readonly("tracking_state", [](const TrackingResult& result) { return result.tracking_state; })
        .def_property_readonly("keypoints", [](const TrackingResult& result) { return result.keypoints; })
        .def_property_readonly("tracked_keypoint_count", [](const TrackingResult& result) { return result.tracked_keypoint_count; });

    py::class_<SystemWrapper>(m, "System")
        .def(
            py::init<
                const std::string&,
                const std::string&,
                ORB_SLAM3::System::eSensor,
                const std::string&,
                const std::string&
            >(),
            py::arg("vocabulary_path"),
            py::arg("settings_path"),
            py::arg("sensor"),
            py::arg("load_atlas_path") = "",
            py::arg("save_atlas_path") = ""
        )
        .def("track_monocular", &SystemWrapper::TrackMonocular, py::arg("image"), py::arg("timestamp"), py::arg("imu_points") = std::vector<ImuPoint>{}, py::arg("filename") = "")
        .def("track_stereo", &SystemWrapper::TrackStereo, py::arg("left_image"), py::arg("right_image"), py::arg("timestamp"), py::arg("imu_points") = std::vector<ImuPoint>{}, py::arg("filename") = "")
        .def("track_rgbd", &SystemWrapper::TrackRgbd, py::arg("image"), py::arg("depth"), py::arg("timestamp"), py::arg("imu_points") = std::vector<ImuPoint>{}, py::arg("filename") = "")
        .def("localize_monocular", &SystemWrapper::LocalizeMonocular, py::arg("image"), py::arg("timestamp"), py::arg("imu_points") = std::vector<ImuPoint>{}, py::arg("filename") = "")
        .def("shutdown", &SystemWrapper::Shutdown)
        .def("reset", &SystemWrapper::Reset)
        .def("reset_active_map", &SystemWrapper::ResetActiveMap)
        .def("activate_localization_mode", &SystemWrapper::ActivateLocalizationMode)
        .def("deactivate_localization_mode", &SystemWrapper::DeactivateLocalizationMode)
        .def("map_changed", &SystemWrapper::MapChanged)
        .def("is_shutdown", &SystemWrapper::IsShutdown)
        .def("is_lost", &SystemWrapper::IsLost)
        .def("is_finished", &SystemWrapper::IsFinished)
        .def("get_time_from_imu_init", &SystemWrapper::GetTimeFromImuInit)
        .def("get_image_scale", &SystemWrapper::GetImageScale)
        .def("is_loading_map", &SystemWrapper::IsLoadingMap)
        .def("get_tracking_state", &SystemWrapper::GetTrackingState)
        .def("save_trajectory_csv", &SystemWrapper::SaveTrajectoryCsv, py::arg("filename"))
        .def("save_trajectory_tum", &SystemWrapper::SaveTrajectoryTum, py::arg("filename"))
        .def("save_keyframe_trajectory_tum", &SystemWrapper::SaveKeyFrameTrajectoryTum, py::arg("filename"))
        .def("save_trajectory_euroc", &SystemWrapper::SaveTrajectoryEuroc, py::arg("filename"))
        .def("save_keyframe_trajectory_euroc", &SystemWrapper::SaveKeyFrameTrajectoryEuroc, py::arg("filename"))
        .def("save_trajectory_kitti", &SystemWrapper::SaveTrajectoryKitti, py::arg("filename"));
}
