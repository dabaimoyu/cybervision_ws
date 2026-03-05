#include <iostream>
#include <chrono>
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "cybervision_interfaces/msg/perception_result.hpp"
#include "qos_profiles.hpp"

/**
 * @brief DDS 通信延迟测试工具
 * 
 * 功能需求:
 * - 测量 Publisher 到 Subscriber 的传输延迟
 * - 统计平均延迟、最大延迟、最小延迟
 * - 验证是否满足 < 60ms 的指标
 */
class LatencyTester : public rclcpp::Node {
public:
    LatencyTester() : Node("latency_tester") {
        // Publisher
        pub_ = this->create_publisher<cybervision_interfaces::msg::PerceptionResult>(
            "perception_results",
            cybervision::QoSProfiles::getSensorDataQoS()
        );
        
        // Subscriber（带时间戳回环）
        sub_ = this->create_subscription<cybervision_interfaces::msg::PerceptionResult>(
            "perception_results",
            cybervision::QoSProfiles::getSensorDataQoS(),
            [this](const cybervision_interfaces::msg::PerceptionResult::SharedPtr msg) {
                auto receive_time = std::chrono::steady_clock::now();
                
                // 计算延迟
                auto send_time = std::chrono::steady_clock::time_point(
                    std::chrono::nanoseconds(msg->header.stamp.sec * 1000000000 + msg->header.stamp.nanosec)
                );
                
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    receive_time - send_time
                ).count() / 1000.0;  // 转换为毫秒
                
                latencies_.push_back(latency);
                
                if (latencies_.size() > window_size_) {
                    latencies_.pop_front();
                }
                
                received_count_++;
                
                if (received_count_ % 50 == 0) {
                    printStats();
                }
            }
        );
        
        RCLCPP_INFO(this->get_logger(), "DDS 延迟测试工具已启动");
        RCLCPP_INFO(this->get_logger(), "将发送 1000 条测试消息...");
    }
    
    void runTest() {
        // 等待订阅者连接
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        RCLCPP_INFO(this->get_logger(), "开始发送测试消息...");
        
        for (int i = 0; i < 1000; i++) {
            auto msg = std::make_unique<cybervision_interfaces::msg::PerceptionResult>();
            msg->header.stamp = this->now();
            msg->frame_id = i;
            
            pub_->publish(std::move(msg));
            
            sent_count_++;
            
            // 以 60 FPS 速率发送
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            
            if (!rclcpp::ok()) break;
        }
        
        // 等待所有消息处理完成
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        printFinalReport();
    }

private:
    void printStats() {
        if (latencies_.empty()) return;
        
        double sum = 0.0;
        double max_val = latencies_[0];
        double min_val = latencies_[0];
        
        for (double lat : latencies_) {
            sum += lat;
            if (lat > max_val) max_val = lat;
            if (lat < min_val) min_val = lat;
        }
        
        double avg = sum / latencies_.size();
        
        std::cout << "\n[延迟统计] 收到：" << received_count_ 
                  << " | 平均：" << avg << " ms"
                  << " | 最大：" << max_val << " ms"
                  << " | 最小：" << min_val << " ms" << std::endl;
    }
    
    void printFinalReport() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "     DDS 通信延迟测试报告               " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "发送消息数：" << sent_count_ << std::endl;
        std::cout << "接收消息数：" << received_count_ << std::endl;
        std::cout << "丢包率：" << (1.0 - received_count_ * 1.0 / sent_count_) * 100 << "%" << std::endl;
        std::cout << std::endl;
        
        if (!latencies_.empty()) {
            double sum = 0.0;
            double max_val = latencies_[0];
            double min_val = latencies_[0];
            
            for (double lat : latencies_) {
                sum += lat;
                if (lat > max_val) max_val = lat;
                if (lat < min_val) min_val = lat;
            }
            
            double avg = sum / latencies_.size();
            
            std::cout << "延迟统计:" << std::endl;
            std::cout << "  平均：" << avg << " ms" << std::endl;
            std::cout << "  最大：" << max_val << " ms" << std::endl;
            std::cout << "  最小：" << min_val << " ms" << std::endl;
            std::cout << std::endl;
            
            // 性能目标检查
            if (avg <= 60.0) {
                std::cout << "✅ 平均延迟达标 (" << avg << " ms <= 60 ms)" << std::endl;
            } else {
                std::cout << "⚠️  平均延迟超标 (" << avg << " ms > 60 ms)" << std::endl;
            }
            
            if (max_val <= 100.0) {
                std::cout << "✅ 最大延迟可接受 (" << max_val << " ms <= 100 ms)" << std::endl;
            } else {
                std::cout << "⚠️  最大延迟过大 (" << max_val << " ms > 100 ms)" << std::endl;
            }
        }
        
        std::cout << "========================================\n" << std::endl;
    }
    
    rclcpp::Publisher<cybervision_interfaces::msg::PerceptionResult>::SharedPtr pub_;
    rclcpp::Subscription<cybervision_interfaces::msg::PerceptionResult>::SharedPtr sub_;
    
    std::deque<double> latencies_;
    size_t window_size_ = 100;
    
    int sent_count_ = 0;
    int received_count_ = 0;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    
    auto node = std::make_shared<LatencyTester>();
    
    // 运行在独立线程
    std::thread test_thread([node]() {
        node->runTest();
        
        // 测试完成后关闭
        rclcpp::shutdown();
    });
    
    // ROS 2 spin
    rclcpp::spin(node->get_node_base_interface());
    
    test_thread.join();
    
    return 0;
}
