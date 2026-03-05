#ifndef CYBERVISION_EDGE_VIDEO_STREAMER_HPP_
#define CYBERVISION_EDGE_VIDEO_STREAMER_HPP_

#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace cybervision {

/**
 * @brief 线程安全的定长队列
 * @tparam T 数据类型
 */
template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t max_size = 5) : max_size_(max_size) {}
    
    bool push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            return false;  // 队列已满，拒绝新数据（防雪崩机制）
        }
        queue_.push(item);
        return true;
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    bool is_full() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size() >= max_size_;
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    size_t max_size_;
};

/**
 * @brief FFmpeg 异步推流器
 * 
 * 功能需求:
 * - FR-1: 封装 VideoStreamer 类，基于 FFmpeg libavcodec 解码视频帧
 * - FR-2: 根据 PTS 进行休眠控制，严格按原帧率拉取
 * - FR-3: 解码后的帧转为 cv::Mat (BGR)，推入线程安全队列
 * - FR-4: 防雪崩机制 - 队列满时主动丢弃新帧
 */
class VideoStreamer {
public:
    VideoStreamer();
    ~VideoStreamer();
    
    /**
     * @brief 初始化视频流
     * @param video_path 视频文件路径
     * @return true 如果初始化成功
     */
    bool init(const std::string& video_path);
    
    /**
     * @brief 启动解码线程
     */
    void start();
    
    /**
     * @brief 停止解码线程
     */
    void stop();
    
    /**
     * @brief 获取下一帧
     * @param frame 输出的 cv::Mat
     * @return true 如果成功获取帧
     */
    bool getNextFrame(cv::Mat& frame);
    
    /**
     * @brief 获取视频 FPS
     */
    double getFPS() const { return fps_; }
    
    /**
     * @brief 获取队列深度
     */
    size_t getQueueSize() const { return queue_.size(); }

private:
    /**
     * @brief 解码线程函数
     */
    void decodeLoop();
    
    std::string video_path_;
    double fps_;
    int64_t frame_pts_;
    
    ThreadSafeQueue<cv::Mat> queue_;
    
    std::thread decode_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // FFmpeg 上下文
    AVFormatContext* format_ctx_;
    AVCodecContext* codec_ctx_;
    int video_stream_index_;
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_VIDEO_STREAMER_HPP_
