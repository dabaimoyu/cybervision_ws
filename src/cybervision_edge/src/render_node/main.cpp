#include <iostream>
#include <memory>
#include <thread>
#include <QApplication>
#include <QTimer>
#include "rclcpp/rclcpp.hpp"
#include "cybervision_interfaces/msg/perception_result.hpp"
#include "double_buffer.hpp"
#include "qos_profiles.hpp"
#include "osg_renderer.hpp"
#include "qt_gui.hpp"

using namespace cybervision;

/**
 * @brief CyberVision 渲染节点主类
 */
class RenderNode : public rclcpp::Node {
public:
    RenderNode() : Node("cybervision_render_node"), 
                   double_buffer_(new DoubleBuffer()),
                   osg_renderer_(new OSGRenderer()),
                   qt_app_(nullptr),
                   qt_window_(nullptr) {
        // 创建 ROS 2 Subscriber（使用 SensorData QoS）
        sub_ = this->create_subscription<cybervision_interfaces::msg::PerceptionResult>(
            "perception_results",
            cybervision::QoSProfiles::getSensorDataQoS(),  // 与 Publisher 匹配
            [this](const cybervision_interfaces::msg::PerceptionResult::SharedPtr msg) {
                perceptionCallback(msg);
            }
        );
        
        RCLCPP_INFO(this->get_logger(), "CyberVision 渲染节点初始化完成!");
        RCLCPP_INFO(this->get_logger(), "🎨 将启动双线程渲染架构:");
        RCLCPP_INFO(this->get_logger(), "  - OSG 渲染线程 (3D 场景)");
        RCLCPP_INFO(this->get_logger(), "  - Qt GUI 线程 (性能监控)");
    }
    
    void run() {
        // 初始化 OSG 渲染器
        if (!osg_renderer_->init()) {
            RCLCPP_ERROR(this->get_logger(), "OSG 渲染器初始化失败!");
            return;
        }
        
        // 启动 ROS 2 spin 线程 (FR-13)
        std::thread ros_thread([this]() {
            rclcpp::spin(this->get_node_base_interface());
        });
        
        // 启动 OSG 渲染线程
        std::thread osg_thread([this]() {
            RCLCPP_INFO(this->get_logger(), "🎨 启动 OSG 渲染线程...");
            osg_renderer_->startRendering();
        });
        
        // 在主线程运行 Qt GUI（避免线程问题）
        RCLCPP_INFO(this->get_logger(), "💻 启动 Qt GUI 线程...");
        
        int argc = 0;
        char** argv = nullptr;
        qt_app_ = new QApplication(argc, argv);
        qt_window_ = new QtGUIWindow();
        qt_window_->show();
        
        // 用 QTimer 驱动主循环，让 Qt 事件循环负责调度（每 16ms 约 60 FPS）
        QTimer* timer = new QTimer();
        int loop_count = 0;
        
        QObject::connect(timer, &QTimer::timeout, [this, &loop_count]() {
            if (!rclcpp::ok()) {
                qt_app_->quit();
                return;
            }
            
            // 从双缓冲区读取最新数据 (FR-14)
            auto frame = double_buffer_->read();
            
            loop_count++;
            if (loop_count % 100 == 0) {
                RCLCPP_INFO(this->get_logger(), "主循环次数：%d, 帧 ID: %ld, 目标数：%zu", 
                           loop_count, frame ? frame->frame_id : -1, frame ? frame->objects.size() : 0);
            }
            
            if (frame && !frame->objects.empty()) {
                static int render_count = 0;
                if (++render_count % 10 == 0) {
                    RCLCPP_INFO(this->get_logger(), "🎨 渲染帧：%d, 目标数：%zu, 帧 ID: %ld", 
                               render_count, frame->objects.size(), frame->frame_id);
                }
                
                // 更新 OSG 场景
                osg_renderer_->updateObjects(frame->objects, frame->frame_id);
                
                // 在 Qt 事件循环中直接更新 GUI（线程安全）
                qt_window_->updatePerformanceData(
                    static_cast<int>(60.0),
                    9.5,
                    frame->inference_time_ms,
                    static_cast<int>(frame->objects.size()),
                    frame->frame_id);
            }
            
            // 每帧结束后交换缓冲区
            swapBuffers(*double_buffer_);
        });
        
        timer->start(16);  // 16ms ≈ 60 FPS
        
        // 启动 Qt 事件循环（阻塞，直到窗口关闭）
        qt_app_->exec();
        
        // 清理
        RCLCPP_INFO(this->get_logger(), "正在关闭渲染节点...");
        
        // 先停止 OSG 渲染
        if (osg_renderer_) {
            osg_renderer_->stopRendering();
        }
        
        // 加入线程
        ros_thread.join();
        osg_thread.join();
    }
    
private:
    /**
     * @brief ROS 2 回调函数 - 写入双缓冲区
     */
    void perceptionCallback(const cybervision_interfaces::msg::PerceptionResult::SharedPtr msg) {
        // 每 10 帧打印一次调试信息
        static int count = 0;
        if (++count % 10 == 0) {
            RCLCPP_INFO(this->get_logger(), "✅ 收到感知结果：帧 ID=%ld, 目标数=%zu", 
                       msg->frame_id, msg->objects.size());
        }
        
        // 创建新的感知帧
        auto frame = std::make_shared<PerceptionFrame>();
        frame->frame_id = msg->frame_id;
        frame->timestamp = rclcpp::Clock().now().seconds();  // 使用当前时间
        frame->objects = msg->objects;
        frame->inference_time_ms = msg->inference_time_ms;
        
        // 写入双缓冲区 (FR-14)
        double_buffer_->write(frame);
        
        if (count % 100 == 0) {
            RCLCPP_INFO(this->get_logger(), "📝 已写入双缓冲区，总帧数：%d", count);
        }
    }
    
    rclcpp::Subscription<cybervision_interfaces::msg::PerceptionResult>::SharedPtr sub_;
    std::shared_ptr<DoubleBuffer> double_buffer_;
    std::shared_ptr<OSGRenderer> osg_renderer_;  // OSG 渲染器
    QApplication* qt_app_;                        // Qt 应用
    QtGUIWindow* qt_window_;                      // Qt 窗口
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    
    auto node = std::make_shared<RenderNode>();
    node->run();
    
    rclcpp::shutdown();
    return 0;
}
