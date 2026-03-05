#ifndef CYBERVISION_EDGE_CAMERA_MODEL_HPP_
#define CYBERVISION_EDGE_CAMERA_MODEL_HPP_

#include <opencv2/opencv.hpp>
#include "trt_engine.hpp"

namespace cybervision {

/**
 * @brief 3D 目标结构 (反投影后的结果)
 */
struct Object3D {
    float x, y, z;          // 3D 位置 (世界坐标系)
    float length, width, height; // 3D 尺寸
    std::string type;       // 目标类型
    float confidence;       // 置信度
    int track_id;           // 跟踪 ID
    
    // 从 2D 检测框转换而来
    static Object3D fromDetection(const Detection& det, 
                                  const cv::Mat& camera_matrix,
                                  float assumed_height = 1.7f);
};

/**
 * @brief 相机模型与反投影变换
 * 
 * 功能需求:
 * - FR-9: AI 输出 2D 像素坐标，假设虚拟相机内参矩阵 K
 * - FR-10: projectTo3D 方法将 2D 映射为 3D 坐标 (X, Y, 0)
 */
class CameraModel {
public:
    CameraModel();
    
    /**
     * @brief 初始化相机参数
     * @param fx 焦距 X
     * @param fy 焦距 Y
     * @param cx 主点 X
     * @param cy 主点 Y
     * @param image_width 图像宽度
     * @param image_height 图像高度
     */
    void init(float fx, float fy, float cx, float cy,
              int image_width, int image_height);
    
    /**
     * @brief 将 2D 边界框反投影到 3D 空间
     * @param detection 2D 检测结果
     * @return Object3D 3D 目标
     */
    Object3D projectTo3D(const Detection& detection);
    
    /**
     * @brief 获取相机内参矩阵
     */
    cv::Mat getCameraMatrix() const { return camera_matrix_; }

private:
    cv::Mat camera_matrix_;      // 3x3 内参矩阵
    cv::Mat dist_coeffs_;        // 畸变系数 (假设为 0)
    int image_width_;
    int image_height_;
    
    // 假设的地平面高度 (相机安装高度)
    float camera_height_;
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_CAMERA_MODEL_HPP_
