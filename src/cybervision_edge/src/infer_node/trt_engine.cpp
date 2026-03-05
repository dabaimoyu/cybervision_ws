#include "trt_engine.hpp"
#include <fstream>
#include <chrono>
#include <iostream>
#include <cuda_runtime_api.h>
#include <algorithm>

namespace {

class TrtLogger : public nvinfer1::ILogger {
public:
    void log(nvinfer1::ILogger::Severity severity, const char* msg) noexcept override {
        if (severity <= nvinfer1::ILogger::Severity::kWARNING) {
            std::cout << "[TRT] " << msg << std::endl;
        }
    }
} gLogger;

} // namespace

namespace cybervision {

TrtEngine::TrtEngine()
    : runtime_(nullptr),
      engine_(nullptr),
      context_(nullptr),
      device_input_(nullptr),
      device_output_(nullptr),
      input_width_(640),
      input_height_(640),
      input_size_(0),
      output_size_(0),
      stream_(nullptr) {
}

TrtEngine::~TrtEngine() {
    // 释放显存 (FR-7 零泄漏保障)
    if (device_input_) {
        cudaFree(device_input_);
    }
    if (device_output_) {
        cudaFree(device_output_);
    }
    
    if (stream_) {
        cudaStreamDestroy(stream_);
    }
    
    if (context_) {
        delete context_;
    }
    if (engine_) {
        delete engine_;
    }
    if (runtime_) {
        delete runtime_;
    }
}

bool TrtEngine::loadEngine(const std::string& engine_path) {
    std::cout << "[INFO] 加载 TensorRT 引擎：" << engine_path << std::endl;
    
    // 读取.engine 文件
    std::ifstream file(engine_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[ERROR] 无法打开 engine 文件：" << engine_path << std::endl;
        return false;
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> model_data(size);
    file.read(model_data.data(), size);
    file.close();
    
    // 创建 runtime
    runtime_ = nvinfer1::createInferRuntime(gLogger);
    if (!runtime_) {
        std::cerr << "[ERROR] 无法创建 TensorRT runtime" << std::endl;
        return false;
    }
    
    // 反序列化引擎
    engine_ = runtime_->deserializeCudaEngine(model_data.data(), size);
    if (!engine_) {
        std::cerr << "[ERROR] 无法反序列化引擎" << std::endl;
        return false;
    }
    
    // 创建执行上下文
    context_ = engine_->createExecutionContext();
    if (!context_) {
        std::cerr << "[ERROR] 无法创建执行上下文" << std::endl;
        return false;
    }
    
    // 获取输入输出尺寸 (TensorRT 10.x API - 使用 I/O Tensors)
    auto num_io_tensors = engine_->getNbIOTensors();
    
    int input_binding = -1;
    int output_binding = -1;
    
    for (int32_t i = 0; i < num_io_tensors; i++) {
        const char* tensor_name = engine_->getIOTensorName(i);
        if (std::string(tensor_name) == "images") {
            input_binding = i;
        } else if (std::string(tensor_name) == "output0") {
            output_binding = i;
        }
    }
    
    if (input_binding == -1 || output_binding == -1) {
        std::cerr << "[ERROR] 无法找到输入输出绑定" << std::endl;
        return false;
    }
    
    auto input_dims = engine_->getTensorShape("images");
    auto output_dims = engine_->getTensorShape("output0");
    
    input_height_ = input_dims.d[2];
    input_width_ = input_dims.d[3];
    
    // 计算显存大小 (FR-6 预分配)
    input_size_ = 3 * input_height_ * input_width_ * sizeof(float);
    output_size_ = output_dims.d[1] * output_dims.d[2] * sizeof(float);
    
    // 预分配显存 (FR-6, FR-7)
    cudaMalloc(&device_input_, input_size_);
    cudaMalloc(&device_output_, output_size_);
    
    // 创建 CUDA 流
    cudaStreamCreate(&stream_);
    
    std::cout << "[INFO] TensorRT 引擎加载成功!" << std::endl;
    std::cout << "[INFO] 输入尺寸：" << input_width_ << "x" << input_height_ << std::endl;
    std::cout << "[INFO] 输入显存：" << input_size_ / 1024 / 1024 << " MB" << std::endl;
    std::cout << "[INFO] 输出显存：" << output_size_ / 1024 / 1024 << " MB" << std::endl;
    
    return true;
}

bool TrtEngine::infer(const cv::Mat& input_image, 
                      std::vector<Detection>& detections,
                      float& inference_time_ms) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 图像预处理
    std::vector<float> host_input(3 * input_height_ * input_width_);
    preprocess(input_image, host_input.data());
    
    // 拷贝到 GPU
    cudaMemcpyAsync(device_input_, host_input.data(), input_size_,
                    cudaMemcpyHostToDevice, stream_);
    
    // 执行推理 (TensorRT 10.x 使用 executeV2)
    void* buffers[2];
    buffers[0] = device_input_;
    buffers[1] = device_output_;
    
    // TensorRT 10.x 使用 executeV2
    bool success = context_->executeV2(buffers);
    
    if (!success) {
        std::cerr << "[ERROR] TensorRT 推理失败" << std::endl;
        return false;
    }
    
    // 从 GPU 拷贝回结果
    std::vector<float> host_output(output_size_ / sizeof(float));
    cudaMemcpyAsync(host_output.data(), device_output_, output_size_,
                    cudaMemcpyDeviceToHost, stream_);
    cudaStreamSynchronize(stream_);
    
    // 解析输出 (YOLOv8 格式)
    int num_proposals = output_size_ / sizeof(float) / (5 + NUM_CLASSES);
    float* output_data = host_output.data();
    
    for (int i = 0; i < num_proposals; i++) {
        float* row = output_data + i * (5 + NUM_CLASSES);
        float x_center = row[0];
        float y_center = row[1];
        float width = row[2];
        float height = row[3];
        float confidence = row[4];
        
        if (confidence < CONF_THRESHOLD) continue;
        
        // 找到最大置信度的类别
        int class_id = 0;
        float max_conf = 0.0f;
        for (int c = 1; c < NUM_CLASSES; c++) {
            if (row[4 + c] > max_conf) {
                max_conf = row[4 + c];
                class_id = c;
            }
        }
        
        // 转换为边界框坐标
        float x1 = (x_center - width / 2.0f) / input_width_ * input_image.cols;
        float y1 = (y_center - height / 2.0f) / input_height_ * input_image.rows;
        float x2 = (x_center + width / 2.0f) / input_width_ * input_image.cols;
        float y2 = (y_center + height / 2.0f) / input_height_ * input_image.rows;
        
        Detection det;
        det.x1 = x1;
        det.y1 = y1;
        det.x2 = x2;
        det.y2 = y2;
        det.confidence = confidence * max_conf;
        det.class_id = class_id;
        
        detections.push_back(det);
    }
    
    // NMS 非极大值抑制 (FR-8)
    nms(detections);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    inference_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    
    return true;
}

void TrtEngine::preprocess(const cv::Mat& input, float* blob) {
    // Resize
    cv::Mat resized;
    cv::resize(input, resized, cv::Size(input_width_, input_height_), 0, 0, cv::INTER_LINEAR);
    
    // BGR -> RGB
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    
    // 归一化并转换为 CHW 格式
    for (int c = 0; c < 3; c++) {
        for (int h = 0; h < input_height_; h++) {
            for (int w = 0; w < input_width_; w++) {
                int index = c * input_height_ * input_width_ + h * input_width_ + w;
                blob[index] = (rgb.at<cv::Vec3b>(h, w)[c] / 255.0f);
            }
        }
    }
}

void TrtEngine::nms(std::vector<Detection>& dets, float nms_threshold) {
    // 按置信度降序排序
    std::sort(dets.begin(), dets.end(), [](const Detection& a, const Detection& b) {
        return a.confidence > b.confidence;
    });
    
    std::vector<int> selected_indices;
    std::vector<bool> suppressed(dets.size(), false);
    
    for (size_t i = 0; i < dets.size(); i++) {
        if (suppressed[i]) continue;
        
        selected_indices.push_back(i);
        
        for (size_t j = i + 1; j < dets.size(); j++) {
            if (suppressed[j]) continue;
            
            // 计算 IoU
            float x1 = std::max(dets[i].x1, dets[j].x1);
            float y1 = std::max(dets[i].y1, dets[j].y1);
            float x2 = std::min(dets[i].x2, dets[j].x2);
            float y2 = std::min(dets[i].y2, dets[j].y2);
            
            float inter_area = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
            float union_area = (dets[i].x2 - dets[i].x1) * (dets[i].y2 - dets[i].y1) +
                              (dets[j].x2 - dets[j].x1) * (dets[j].y2 - dets[j].y1) - inter_area;
            
            float iou = union_area > 0 ? inter_area / union_area : 0;
            
            if (iou > nms_threshold) {
                suppressed[j] = true;
            }
        }
    }
    
    // 保留选中的检测结果
    std::vector<Detection> filtered_dets;
    for (int idx : selected_indices) {
        filtered_dets.push_back(dets[idx]);
    }
    dets = filtered_dets;
}

} // namespace cybervision
