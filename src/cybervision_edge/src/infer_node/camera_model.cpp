#include "camera_model.hpp"
#include <iostream>

namespace cybervision {

Object3D Object3D::fromDetection(const Detection& det, 
                                  const cv::Mat& camera_matrix,
                                  float assumed_height) {
    // 计算目标底边中心点
    float u = (det.x1 + det.x2) / 2.0f;
    float v = det.y2;  // 使用底部边缘
    
    // 反投影到 3D
    cv::Mat pixel_point = (cv::Mat_<float>(3, 1) << u, v, 1.0f);
    cv::Mat camera_inv = camera_matrix.inv();
    cv::Mat ray = camera_inv * pixel_point;
    
    // 假设地平面 Z=0，计算 X,Y
    float fx = camera_matrix.at<float>(0, 0);
    float fy = camera_matrix.at<float>(1, 1);
    float cx = camera_matrix.at<float>(0, 2);
    float cy = camera_matrix.at<float>(1, 2);
    
    // 小孔成像原理反投影
    float Z = assumed_height;  // 假设目标高度
    float X = (u - cx) * Z / fx;
    float Y = (v - cy) * Z / fy;
    
    Object3D obj;
    obj.x = X;
    obj.y = Y;
    obj.z = 0.0f;  // 地平面上
    obj.height = assumed_height;
    obj.width = 0.5f;   // 假设行人宽度
    obj.length = 0.5f;  // 假设行人深度
    obj.type = "pedestrian";
    obj.confidence = det.confidence;
    obj.track_id = -1;
    
    return obj;
}

CameraModel::CameraModel()
    : image_width_(1920),
      image_height_(1080),
      camera_height_(1.5f) {
    // 初始化畸变系数为 0
    dist_coeffs_ = cv::Mat::zeros(5, 1, CV_64F);
}

void CameraModel::init(float fx, float fy, float cx, float cy,
                       int image_width, int image_height) {
    image_width_ = image_width;
    image_height_ = image_height;
    
    // 构建相机内参矩阵 K
    camera_matrix_ = (cv::Mat_<float>(3, 3) <<
        fx, 0.0f, cx,
        0.0f, fy, cy,
        0.0f, 0.0f, 1.0f);
    
    std::cout << "[INFO] 相机模型初始化完成" << std::endl;
    std::cout << "[INFO] 焦距：fx=" << fx << ", fy=" << fy << std::endl;
    std::cout << "[INFO] 主点：cx=" << cx << ", cy=" << cy << std::endl;
}

Object3D CameraModel::projectTo3D(const Detection& detection) {
    // FR-9, FR-10: 将 2D 像素坐标映射到 3D 空间
    
    // 获取边界框底部中心点
    float u = (detection.x1 + detection.x2) / 2.0f;
    float v = detection.y2;  // 使用底部作为接触地面的点
    
    // 相机内参
    float fx = camera_matrix_.at<float>(0, 0);
    float fy = camera_matrix_.at<float>(1, 1);
    float cx = camera_matrix_.at<float>(0, 2);
    float cy = camera_matrix_.at<float>(1, 2);
    
    // 假设地平面 Z=0，相机高度已知
    // 根据小孔成像模型：X_cam = (u - cx) * Z / fx, Y_cam = (v - cy) * Z / fy
    // 对于地平面上的点，Y_cam = camera_height_
    
    // 计算深度 Z
    // Y_cam = (v - cy) * Z / fy => Z = Y_cam * fy / (v - cy)
    // 注意：图像坐标系中 v 向下为正，世界坐标系中 Y 向上为正
    float Y_world = camera_height_;  // 相机安装高度
    float Z = Y_world * fy / (v - cy);
    
    // 计算 X 和 Y 坐标
    float X = (u - cx) * Z / fx;
    float Y = -Y_world;  // 地平面上，Y 为负值（相机下方）
    
    // 根据类别确定目标尺寸
    Object3D obj3d;
    obj3d.x = X;
    obj3d.y = Y;
    obj3d.z = 0.0f;
    obj3d.confidence = detection.confidence;
    obj3d.track_id = -1;
    
    // 根据类别设置类型和尺寸
    if (detection.class_id == 0) {  // person
        obj3d.type = "pedestrian";
        obj3d.height = 1.7f;
        obj3d.width = 0.6f;
        obj3d.length = 0.5f;
    } else if (detection.class_id == 2 || detection.class_id == 3 || 
               detection.class_id == 5 || detection.class_id == 7) {  // car, truck, bus, train
        obj3d.type = "vehicle";
        obj3d.height = 1.5f;
        obj3d.width = 1.8f;
        obj3d.length = 4.0f;
    } else {
        obj3d.type = "unknown";
        obj3d.height = 1.0f;
        obj3d.width = 1.0f;
        obj3d.length = 1.0f;
    }
    
    return obj3d;
}

} // namespace cybervision
