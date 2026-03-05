#ifndef CYBERVISION_EDGE_DOUBLE_BUFFER_HPP_
#define CYBERVISION_EDGE_DOUBLE_BUFFER_HPP_

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include "cybervision_interfaces/msg/perception_result.hpp"

namespace cybervision {

/**
 * @brief 感知帧数据结构 (用于双缓冲)
 */
struct PerceptionFrame {
    int64_t frame_id;
    double timestamp;
    std::vector<cybervision_interfaces::msg::DetectedObject3D> objects;
    float inference_time_ms;
};

/**
 * @brief 双缓冲机制实现
 * 
 * 功能需求:
 * - FR-13: 后台线程运行 rclcpp::spin 接收数据
 * - FR-14: 无锁优化/双缓冲机制
 *   - ROS 2 线程（写者）写入 buffer_A
 *   - OSG 渲染线程（读者）从 buffer_B 读取
 *   - 每帧通过原子操作交换指针
 */
class DoubleBuffer {
public:
    DoubleBuffer();
    
    /**
     * @brief 写入新帧 (ROS 2 回调调用)
     */
    void write(std::shared_ptr<PerceptionFrame> frame);
    
    /**
     * @brief 读取最新帧 (OSG 渲染线程调用)
     */
    std::shared_ptr<PerceptionFrame> read();
    
    /**
     * @brief 获取帧计数
     */
    int64_t getFrameCount() const { return frame_count_; }

private:
    std::shared_ptr<PerceptionFrame> buffer_a_;
    std::shared_ptr<PerceptionFrame> buffer_b_;
    
    // 当前写缓冲区和读缓冲区标识
    std::atomic<int> write_buffer_;  // 0=A, 1=B
    std::atomic<int> read_buffer_;   // 0=A, 1=B
    
    mutable std::mutex swap_mutex_;  // 改为 mutable 以便 const 方法也能用
    int64_t frame_count_;
    
public:
    // 提供公共访问方法
    std::mutex& getMutex() { return swap_mutex_; }
    int getWriteBuffer() const { return write_buffer_.load(); }
    int getReadBuffer() const { return read_buffer_.load(); }
    void setWriteBuffer(int val) { write_buffer_.store(val); }
    void setReadBuffer(int val) { read_buffer_.store(val); };
};

/**
 * @brief 交换双缓冲区（在主循环中调用）
 */
void swapBuffers(DoubleBuffer& buffer);

} // namespace cybervision

#endif // CYBERVISION_EDGE_DOUBLE_BUFFER_HPP_
