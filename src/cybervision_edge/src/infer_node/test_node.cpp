#include <iostream>
#include <string>
#include <chrono>
#include <random>
#include "rclcpp/rclcpp.hpp"
#include "cybervision_interfaces/msg/perception_result.hpp"
#include "cybervision_interfaces/msg/detected_object3_d.hpp"
#include "qos_profiles.hpp"
#include "performance_monitor.hpp"

using namespace cybervision;

/**
 * @brief 简化的测试推理节点（不需要实际模型和视频）
 * 
 * 用于验证 ROS 2 通信架构和性能监控系统
 */
class TestInferNode : public rclcpp::Node {
public:
    TestInferNode() : Node("cybervision_infer_node_test"), 
                      perf_monitor_(std::make_shared<PerformanceMonitor>(60)) {
        // 使用 SensorData QoS (FR-4, FR-5)
        pub_ = this->create_publisher<cybervision_interfaces::msg::PerceptionResult>(
            "perception_results", 
            cybervision::QoSProfiles::getSensorDataQoS()
        );
        
        RCLCPP_INFO(this->get_logger(), "===========================================");
        RCLCPP_INFO(this->get_logger(), "CyberVision Edge - AI 推理节点 (测试模式)");
        RCLCPP_INFO(this->get_logger(), "QoS 策略：BestEffort + Volatile");
        RCLCPP_INFO(this->get_logger(), "目标 FPS: 60");
        RCLCPP_INFO(this->get_logger(), "===========================================");
        
        // 创建定时器模拟 AI 推理（每 16.67ms 一帧，即 60FPS）
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(16),
            std::bind(&TestInferNode::timerCallback, this)
        );
        
        frame_id_ = 0;
    }

private:
    void timerCallback() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 模拟 AI 推理耗时 (10-15ms 随机)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> inference_dist(10.0, 15.0);
        double simulated_inference_time = inference_dist(gen);
        
        std::this_thread::sleep_for(std::chrono::microseconds(
            static_cast<int>(simulated_inference_time * 1000)
        ));
        
        // 模拟检测到的目标 (1-5 个)
        std::uniform_int_distribution<> num_objects_dist(1, 5);
        int num_objects = num_objects_dist(gen);
        
        // 创建感知结果消息
        auto perception_msg = std::make_unique<cybervision_interfaces::msg::PerceptionResult>();
        perception_msg->timestamp = this->now();
        perception_msg->frame_id = frame_id_++;
        perception_msg->inference_time_ms = static_cast<float>(simulated_inference_time);
        perception_msg->camera_frame = "camera_front";
        
        // 添加模拟的 3D 检测结果
        std::uniform_real_distribution<> pos_dist(-10.0, 10.0);
        std::uniform_real_distribution<> size_dist(0.5, 2.0);
        std::uniform_real_distribution<> confidence_dist(0.8, 0.99);
        
        for (int i = 0; i < num_objects; i++) {
            cybervision_interfaces::msg::DetectedObject3D obj;
            
            // 3D 位置 (世界坐标系)
            obj.x = pos_dist(gen);
            obj.y = pos_dist(gen);
            obj.z = pos_dist(gen);
            
            // 3D 尺寸
            obj.length = size_dist(gen);
            obj.width = size_dist(gen);
            obj.height = size_dist(gen);
            
            // 目标类型
            std::vector<std::string> types = {"car", "pedestrian", "truck", "bus", "bicycle"};
            obj.object_type = types[gen() % types.size()];
            
            // 置信度
            obj.confidence = confidence_dist(gen);
            
            // 跟踪 ID
            obj.track_id = frame_id_ * 10 + i;
            
            perception_msg->objects.push_back(obj);
        }
        
        // 记录性能数据 (FR-9, NFR 指标)
        perf_monitor_->recordInferenceTime(simulated_inference_time);
        perf_monitor_->recordFrame();
        
        // 模拟端到端延迟（推理 + 传输）
        auto end_time = std::chrono::high_resolution_clock::now();
        double e2e_latency = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        perf_monitor_->recordEndToEndLatency(e2e_latency);
        
        // 发布消息
        pub_->publish(std::move(perception_msg));
        
        // 每 30 帧打印性能报告
        if (frame_id_ % 30 == 0) {
            printPerformanceReport();
        }
    }
    
    void printPerformanceReport() {
        RCLCPP_INFO(this->get_logger(), "\n");
        RCLCPP_INFO(this->get_logger(), "╔════════════════════════════════════════╗");
        RCLCPP_INFO(this->get_logger(), "║   Performance Report (Frame %6d)    ║", frame_id_);
        RCLCPP_INFO(this->get_logger(), "╠════════════════════════════════════════╨");
        
        auto stats = perf_monitor_->getStats();
        
        // FPS
        if (stats.fps >= 60.0) {
            RCLCPP_INFO(this->get_logger(), "║ FPS: %.1f ✅ (Target: ≥60)", stats.fps);
        } else {
            RCLCPP_INFO(this->get_logger(), "║ FPS: %.1f ⚠️  (Target: ≥60)", stats.fps);
        }
        
        // 端到端延迟
        if (stats.avg_end_to_end_latency_ms < 60.0) {
            RCLCPP_INFO(this->get_logger(), "║ End-to-End Latency: %.2fms ✅ (Target: <60ms)", stats.avg_end_to_end_latency_ms);
        } else {
            RCLCPP_INFO(this->get_logger(), "║ End-to-End Latency: %.2fms ⚠️  (Target: <60ms)", stats.avg_end_to_end_latency_ms);
        }
        
        // 推理耗时
        if (stats.avg_inference_time_ms < 16.67) {
            RCLCPP_INFO(this->get_logger(), "║ Inference Time: %.2fms ✅ (Target: <16.67ms)", stats.avg_inference_time_ms);
        } else {
            RCLCPP_INFO(this->get_logger(), "║ Inference Time: %.2fms ⚠️  (Target: <16.67ms)", stats.avg_inference_time_ms);
        }
        
        RCLCPP_INFO(this->get_logger(), "╠════════════════════════════════════════╨");
        RCLCPP_INFO(this->get_logger(), "║ NFR Compliance: %s", 
                   (stats.fps >= 60.0 && stats.avg_end_to_end_latency_ms < 60.0) ? "✅ PASS" : "⚠️  FAIL");
        RCLCPP_INFO(this->get_logger(), "╚═══════════════════════════════════════════════════");
    }
    
    rclcpp::Publisher<cybervision_interfaces::msg::PerceptionResult>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::shared_ptr<PerformanceMonitor> perf_monitor_;
    int64_t frame_id_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    
    try {
        auto node = std::make_shared<TestInferNode>();
        rclcpp::spin(node);
    } catch (const std::exception& e) {
        RCLCPP_ERROR(rclcpp::get_logger("cybervision_infer_node_test"), "节点异常：%s", e.what());
    }
    
    rclcpp::shutdown();
    return 0;
}
