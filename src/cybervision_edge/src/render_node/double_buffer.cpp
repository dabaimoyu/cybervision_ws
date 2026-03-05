#include "double_buffer.hpp"
#include <iostream>

namespace cybervision {

DoubleBuffer::DoubleBuffer()
    : write_buffer_(0),
      read_buffer_(1),
      frame_count_(0) {
    // 初始化两个缓冲区
    buffer_a_ = std::make_shared<PerceptionFrame>();
    buffer_b_ = std::make_shared<PerceptionFrame>();
}

void DoubleBuffer::write(std::shared_ptr<PerceptionFrame> frame) {
    // ROS 2 线程（写者）写入当前写缓冲区
    std::lock_guard<std::mutex> lock(swap_mutex_);
    
    int current_write = write_buffer_.load();
    
    // 写入对应的缓冲区
    if (current_write == 0) {
        *buffer_a_ = *frame;
    } else {
        *buffer_b_ = *frame;
    }
    
    frame_count_++;
}

std::shared_ptr<PerceptionFrame> DoubleBuffer::read() {
    // OSG 渲染线程（读者）读取当前读缓冲区
    int current_read = read_buffer_.load();
    
    if (current_read == 0) {
        return buffer_a_;
    } else {
        return buffer_b_;
    }
}

// 注意：实际的缓冲区交换应该在渲染循环中调用
// 这里提供一个简单的交换方法示例
void swapBuffers(DoubleBuffer& buffer) {
    std::lock_guard<std::mutex> lock(buffer.getMutex());
    
    // 原子操作交换读写缓冲区
    int old_write = buffer.getWriteBuffer();
    int new_write = 1 - old_write;
    
    // 先切换写缓冲区（下一次写入另一个缓冲区）
    buffer.setWriteBuffer(new_write);
    // 然后切换读缓冲区到刚写完的缓冲区（即原来的写缓冲区）
    buffer.setReadBuffer(old_write);
}

} // namespace cybervision
