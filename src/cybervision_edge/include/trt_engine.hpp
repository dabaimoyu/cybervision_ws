#ifndef CYBERVISION_EDGE_TRAFFIC_LIGHT_HPP_
#define CYBERVISION_EDGE_TRAFFIC_LIGHT_HPP_

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <NvInfer.h>

namespace cybervision {

/**
 * @brief 检测到的目标结构
 */
struct Detection {
    float x1, y1, x2, y2;  // 边界框坐标
    float confidence;       // 置信度
    int class_id;           // 类别 ID
    std::string class_name; // 类别名称
};

/**
 * @brief TensorRT 推理引擎封装
 * 
 * 功能需求:
 * - FR-5: 将 YOLOv8 的.onnx 模型编译为.engine 文件
 * - FR-6: 封装 TrtEngine 类，预分配显存空间
 * - FR-7: 零泄漏保障 - 循环中严禁 new/delete
 * - FR-8: 手工实现 NMS 后处理
 */
class TrtEngine {
public:
    TrtEngine();
    ~TrtEngine();
    
    /**
     * @brief 加载 TensorRT 引擎
     * @param engine_path .engine 文件路径
     * @return true 如果加载成功
     */
    bool loadEngine(const std::string& engine_path);
    
    /**
     * @brief 执行推理
     * @param input_image 输入图像 (BGR)
     * @param detections 输出检测结果
     * @param inference_time_ms 推理耗时 (毫秒)
     * @return true 如果推理成功
     */
    bool infer(const cv::Mat& input_image, 
               std::vector<Detection>& detections,
               float& inference_time_ms);
    
    /**
     * @brief 获取输入图像尺寸
     */
    int getInputWidth() const { return input_width_; }
    int getInputHeight() const { return input_height_; }

private:
    /**
     * @brief 图像预处理 (归一化、HWC->CHW、BGR->RGB)
     */
    void preprocess(const cv::Mat& input, float* blob);
    
    /**
     * @brief NMS 非极大值抑制
     */
    void nms(std::vector<Detection>& dets, float nms_threshold = 0.45);
    
    // TensorRT 对象
    nvinfer1::IRuntime* runtime_;
    nvinfer1::ICudaEngine* engine_;
    nvinfer1::IExecutionContext* context_;
    
    // 预分配的显存 (FR-6, FR-7)
    void* device_input_;
    void* device_output_;
    
    // 输入输出尺寸
    int input_width_;
    int input_height_;
    size_t input_size_;
    size_t output_size_;
    
    // CUDA 流
    cudaStream_t stream_;
    
    // YOLOv8 参数
    static constexpr int NUM_CLASSES = 80;
    static constexpr float CONF_THRESHOLD = 0.5f;
    static constexpr float NMS_THRESHOLD = 0.45f;
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_TRAFFIC_LIGHT_HPP_
