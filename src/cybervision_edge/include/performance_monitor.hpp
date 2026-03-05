#ifndef CYBERVISION_EDGE_PERFORMANCE_MONITOR_HPP_
#define CYBERVISION_EDGE_PERFORMANCE_MONITOR_HPP_

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <atomic>

namespace cybervision {

/**
 * @brief 性能统计数据结构
 */
struct PerformanceStats {
    double avg_inference_time_ms = 0.0;
    double max_inference_time_ms = 0.0;
    double min_inference_time_ms = 1000.0;
    double avg_end_to_end_latency_ms = 0.0;
    double fps = 0.0;
    int total_frames = 0;
};

/**
 * @brief 性能监控器
 * 
 * 功能需求:
 * - NFR-1: 端到端延迟监控 (< 60ms)
 * - NFR-2: 推理速度监控 (>= 60 FPS)
 * - NFR-4: CPU 占用率监控
 */
class PerformanceMonitor {
public:
    PerformanceMonitor(size_t window_size = 60);
    
    /**
     * @brief 记录推理耗时
     */
    void recordInferenceTime(double time_ms);
    
    /**
     * @brief 记录端到端延迟
     */
    void recordEndToEndLatency(double latency_ms);
    
    /**
     * @brief 记录帧处理完成
     */
    void recordFrame();
    
    /**
     * @brief 获取当前性能统计
     */
    PerformanceStats getStats() const;
    
    /**
     * @brief 打印性能报告到日志
     */
    void printReport() const;
    
    /**
     * @brief 检查是否满足性能指标
     * @return true 如果所有指标都达标
     */
    bool checkPerformanceTargets() const;

private:
    mutable std::mutex mutex_;
    
    std::deque<double> inference_times_;
    std::deque<double> latencies_;
    std::deque<std::chrono::steady_clock::time_point> frame_timestamps_;
    
    size_t window_size_;
    int total_frames_;
    std::chrono::steady_clock::time_point start_time_;
    
    // 性能目标值
    static constexpr double TARGET_MAX_LATENCY_MS = 60.0;
    static constexpr double TARGET_MIN_FPS = 60.0;
    static constexpr double TARGET_MAX_INFERENCE_TIME_MS = 16.67;  // 1/60秒
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_PERFORMANCE_MONITOR_HPP_
