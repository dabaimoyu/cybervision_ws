#include "performance_monitor.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>

namespace cybervision {

PerformanceMonitor::PerformanceMonitor(size_t window_size)
    : window_size_(window_size),
      total_frames_(0),
      start_time_(std::chrono::steady_clock::now()) {
}

void PerformanceMonitor::recordInferenceTime(double time_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    inference_times_.push_back(time_ms);
    if (inference_times_.size() > window_size_) {
        inference_times_.pop_front();
    }
}

void PerformanceMonitor::recordEndToEndLatency(double latency_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    latencies_.push_back(latency_ms);
    if (latencies_.size() > window_size_) {
        latencies_.pop_front();
    }
}

void PerformanceMonitor::recordFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    frame_timestamps_.push_back(std::chrono::steady_clock::now());
    total_frames_++;
    
    // 保持窗口大小
    if (frame_timestamps_.size() > window_size_) {
        frame_timestamps_.pop_front();
    }
}

PerformanceStats PerformanceMonitor::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PerformanceStats stats;
    stats.total_frames = total_frames_;
    
    // 计算推理时间统计
    if (!inference_times_.empty()) {
        stats.avg_inference_time_ms = 
            std::accumulate(inference_times_.begin(), inference_times_.end(), 0.0) 
            / inference_times_.size();
        stats.max_inference_time_ms = *std::max_element(inference_times_.begin(), inference_times_.end());
        stats.min_inference_time_ms = *std::min_element(inference_times_.begin(), inference_times_.end());
    }
    
    // 计算延迟统计
    if (!latencies_.empty()) {
        stats.avg_end_to_end_latency_ms = 
            std::accumulate(latencies_.begin(), latencies_.end(), 0.0) 
            / latencies_.size();
    }
    
    // 计算 FPS
    if (frame_timestamps_.size() >= 2) {
        auto first = frame_timestamps_.front();
        auto last = frame_timestamps_.back();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(last - first).count();
        
        if (duration > 0) {
            stats.fps = (frame_timestamps_.size() - 1) * 1000.0 / duration;
        }
    } else {
        // 如果帧数不足，使用总时间计算
        auto now = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        
        if (total_duration > 0) {
            stats.fps = total_frames_ / static_cast<double>(total_duration);
        }
    }
    
    return stats;
}

void PerformanceMonitor::printReport() const {
    auto stats = getStats();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "       CyberVision 性能监控报告         " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总处理帧数：" << stats.total_frames << std::endl;
    std::cout << "实时 FPS:   " << stats.fps << std::endl;
    std::cout << std::endl;
    std::cout << "推理耗时:" << std::endl;
    std::cout << "  平均：" << stats.avg_inference_time_ms << " ms" << std::endl;
    std::cout << "  最大：" << stats.max_inference_time_ms << " ms" << std::endl;
    std::cout << "  最小：" << stats.min_inference_time_ms << " ms" << std::endl;
    std::cout << std::endl;
    std::cout << "端到端延迟：" << stats.avg_end_to_end_latency_ms << " ms" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 性能目标检查
    bool all_good = checkPerformanceTargets();
    
    if (!all_good) {
        std::cout << "⚠️  警告：部分性能指标未达标!" << std::endl;
        
        if (stats.fps < TARGET_MIN_FPS) {
            std::cout << "  ❌ FPS: " << stats.fps << " < " << TARGET_MIN_FPS << std::endl;
        } else {
            std::cout << "  ✅ FPS: " << stats.fps << " >= " << TARGET_MIN_FPS << std::endl;
        }
        
        if (stats.avg_end_to_end_latency_ms > TARGET_MAX_LATENCY_MS) {
            std::cout << "  ❌ 延迟：" << stats.avg_end_to_end_latency_ms 
                      << " ms > " << TARGET_MAX_LATENCY_MS << " ms" << std::endl;
        } else {
            std::cout << "  ✅ 延迟：" << stats.avg_end_to_end_latency_ms 
                      << " ms <= " << TARGET_MAX_LATENCY_MS << " ms" << std::endl;
        }
        
        if (stats.avg_inference_time_ms > TARGET_MAX_INFERENCE_TIME_MS) {
            std::cout << "  ❌ 推理时间：" << stats.avg_inference_time_ms 
                      << " ms > " << TARGET_MAX_INFERENCE_TIME_MS << " ms" << std::endl;
        } else {
            std::cout << "  ✅ 推理时间：" << stats.avg_inference_time_ms 
                      << " ms <= " << TARGET_MAX_INFERENCE_TIME_MS << " ms" << std::endl;
        }
    } else {
        std::cout << "✅ 所有性能指标均达标!" << std::endl;
    }
    
    std::cout << "========================================\n" << std::endl;
}

bool PerformanceMonitor::checkPerformanceTargets() const {
    auto stats = getStats();
    
    return stats.fps >= TARGET_MIN_FPS &&
           stats.avg_end_to_end_latency_ms <= TARGET_MAX_LATENCY_MS &&
           stats.avg_inference_time_ms <= TARGET_MAX_INFERENCE_TIME_MS;
}

} // namespace cybervision
