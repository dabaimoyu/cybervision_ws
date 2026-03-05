#include <iostream>
#include <string>
#include <chrono>
#include "rclcpp/rclcpp.hpp"
#include "cybervision_interfaces/msg/perception_result.hpp"
#include "video_streamer.hpp"
#include "trt_engine.hpp"
#include "camera_model.hpp"
#include "performance_monitor.hpp"
#include "qos_profiles.hpp"

using namespace cybervision;

/**
 * @brief CyberVision 推理节点主类
 */
class InferNode : public rclcpp::Node {
public:
    InferNode() : Node("cybervision_infer_node"), 
                  perf_monitor_(std::make_shared<PerformanceMonitor>(60)) {
        // 声明参数
        this->declare_parameter<std::string>("video_path", "");
        this->declare_parameter<std::string>("engine_path", "");
        
        std::string video_path = this->get_parameter("video_path").as_string();
        std::string engine_path = this->get_parameter("engine_path").as_string();
        
        if (video_path.empty() || engine_path.empty()) {
            RCLCPP_ERROR(this->get_logger(), "必须指定 video_path 和 engine_path 参数!");
            exit(1);
        }
        
        // 初始化组件
        RCLCPP_INFO(this->get_logger(), "初始化 VideoStreamer...");
        if (!streamer_.init(video_path)) {
            RCLCPP_ERROR(this->get_logger(), "VideoStreamer 初始化失败!");
            exit(1);
        }
        
        RCLCPP_INFO(this->get_logger(), "加载 TensorRT 引擎...");
        if (!engine_.loadEngine(engine_path)) {
            RCLCPP_ERROR(this->get_logger(), "TensorRT 引擎加载失败!");
            exit(1);
        }
        
        // 初始化相机模型 (假设参数)
        camera_model_.init(
            915.0f,   // fx
            915.0f,   // fy
            960.0f,   // cx (1920/2)
            540.0f,   // cy (1080/2)
            1920,     // image_width
            1080      // image_height
        );
        
        // 创建 ROS 2 Publisher（使用 SensorData QoS）
        pub_ = this->create_publisher<cybervision_interfaces::msg::PerceptionResult>(
            "perception_results", 
            cybervision::QoSProfiles::getSensorDataQoS()  // FR-11: SensorData QoS
        );
        
        frame_id_ = 0;
        running_ = true;
        
        RCLCPP_INFO(this->get_logger(), "CyberVision 推理节点初始化完成!");
    }
    
    void run() {
        streamer_.start();
        
        cv::Mat frame;
        while (rclcpp::ok() && running_) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // 获取视频帧
            if (!streamer_.getNextFrame(frame)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            // AI 推理
            std::vector<Detection> detections;
            float inference_time_ms = 0.0f;
            if (!engine_.infer(frame, detections, inference_time_ms)) {
                RCLCPP_WARN(this->get_logger(), "推理失败，跳过此帧");
                continue;
            }
            
            // 记录性能数据
            perf_monitor_->recordInferenceTime(inference_time_ms);
            
            // 反投影到 3D
            auto perception_msg = std::make_unique<cybervision_interfaces::msg::PerceptionResult>();
            perception_msg->timestamp = this->now();  // 使用 timestamp 字段
            perception_msg->frame_id = frame_id_++;
            perception_msg->inference_time_ms = inference_time_ms;
            
            for (const auto& det : detections) {
                Object3D obj3d = camera_model_.projectTo3D(det);
                
                cybervision_interfaces::msg::DetectedObject3D obj_msg;
                obj_msg.x = obj3d.x;
                obj_msg.y = obj3d.y;
                obj_msg.z = obj3d.z;
                obj_msg.length = obj3d.length;
                obj_msg.width = obj3d.width;
                obj_msg.height = obj3d.height;
                obj_msg.object_type = obj3d.type;
                obj_msg.confidence = obj3d.confidence;
                obj_msg.track_id = obj3d.track_id;
                
                perception_msg->objects.push_back(obj_msg);
            }
            
            // 发布消息
            pub_->publish(std::move(perception_msg));
            
            // 记录帧完成
            perf_monitor_->recordFrame();
            
            // 性能监控
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            // 计算端到端延迟（包括 ROS 2 传输时间）
            double e2e_latency = duration + 5.0;  // 估算 DDS 传输延迟
            perf_monitor_->recordEndToEndLatency(e2e_latency);
            
            if (frame_id_ % 30 == 0) {
                RCLCPP_INFO_STREAM(this->get_logger(), 
                    "帧率：" << frame_id_ << " | "
                    "检测目标数：" << detections.size() << " | "
                    "推理耗时：" << inference_time_ms << "ms | "
                    "总耗时：" << duration << "ms");
                
                // 每 30 帧打印一次性能报告
                perf_monitor_->printReport();
            }
        }
        
        streamer_.stop();
    }
    
    void shutdown() {
        running_ = false;
    }

private:
    VideoStreamer streamer_;
    TrtEngine engine_;
    CameraModel camera_model_;
    std::shared_ptr<PerformanceMonitor> perf_monitor_;
    
    rclcpp::Publisher<cybervision_interfaces::msg::PerceptionResult>::SharedPtr pub_;
    
    int64_t frame_id_;
    bool running_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    
    auto node = std::make_shared<InferNode>();
    
    // 运行在独立线程
    std::thread run_thread([node]() {
        node->run();
    });
    
    // ROS 2 spin
    rclcpp::spin(node->get_node_base_interface());
    
    run_thread.join();
    rclcpp::shutdown();
    
    return 0;
}
