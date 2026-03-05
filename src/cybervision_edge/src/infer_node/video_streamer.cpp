#include "video_streamer.hpp"
#include <iostream>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace cybervision {

VideoStreamer::VideoStreamer()
    : fps_(0.0),
      frame_pts_(0),
      queue_(5),  // 定长队列深度为 5
      running_(false),
      initialized_(false),
      format_ctx_(nullptr),
      codec_ctx_(nullptr),
      video_stream_index_(-1) {
}

VideoStreamer::~VideoStreamer() {
    stop();
    
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
    }
}

bool VideoStreamer::init(const std::string& video_path) {
    video_path_ = video_path;
    
    // 打开视频文件
    if (avformat_open_input(&format_ctx_, video_path.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "[ERROR] 无法打开视频文件：" << video_path << std::endl;
        return false;
    }
    
    // 获取流信息
    if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
        std::cerr << "[ERROR] 无法找到流信息" << std::endl;
        return false;
    }
    
    // 查找视频流
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }
    
    if (video_stream_index_ == -1) {
        std::cerr << "[ERROR] 未找到视频流" << std::endl;
        return false;
    }
    
    // 获取帧率
    AVRational frame_rate = format_ctx_->streams[video_stream_index_]->avg_frame_rate;
    fps_ = av_q2d(frame_rate);
    std::cout << "[INFO] 视频 FPS: " << fps_ << std::endl;
    
    // 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(
        format_ctx_->streams[video_stream_index_]->codecpar->codec_id);
    if (!codec) {
        std::cerr << "[ERROR] 不支持的编解码器" << std::endl;
        return false;
    }
    
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_ctx_, 
            format_ctx_->streams[video_stream_index_]->codecpar) < 0) {
        std::cerr << "[ERROR] 无法复制编解码器参数" << std::endl;
        return false;
    }
    
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        std::cerr << "[ERROR] 无法打开编解码器" << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

void VideoStreamer::start() {
    if (!initialized_) {
        std::cerr << "[ERROR] VideoStreamer 未初始化" << std::endl;
        return;
    }
    
    running_ = true;
    decode_thread_ = std::thread(&VideoStreamer::decodeLoop, this);
}

void VideoStreamer::stop() {
    running_ = false;
    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }
}

bool VideoStreamer::getNextFrame(cv::Mat& frame) {
    return queue_.pop(frame);
}

void VideoStreamer::decodeLoop() {
    AVPacket* packet = av_packet_alloc();
    AVFrame* av_frame = av_frame_alloc();
    AVFrame* rgb_frame = av_frame_alloc();
    
    // 创建颜色转换上下文
    struct SwsContext* sws_ctx = sws_getContext(
        codec_ctx_->width, codec_ctx_->height, codec_ctx_->pix_fmt,
        codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    // 分配 RGB 缓冲区
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, 
        codec_ctx_->width, codec_ctx_->height, 1);
    uint8_t* out_buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, out_buffer,
        AV_PIX_FMT_BGR24, codec_ctx_->width, codec_ctx_->height, 1);
    
    int64_t last_pts = 0;
    
    while (running_) {
        int ret = av_read_frame(format_ctx_, packet);
        if (ret < 0) {
            // 读取完毕，循环播放或退出
            av_seek_frame(format_ctx_, video_stream_index_, 0, AVSEEK_FLAG_BACKWARD);
            last_pts = 0;
            continue;
        }
        
        if (packet->stream_index != video_stream_index_) {
            av_packet_unref(packet);
            continue;
        }
        
        // PTS 时间控制 (FR-2)
        if (packet->pts != AV_NOPTS_VALUE) {
            if (last_pts != 0) {
                double sleep_time = (packet->pts - last_pts) * 
                    av_q2d(format_ctx_->streams[video_stream_index_]->time_base);
                
                if (sleep_time > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(sleep_time));
                }
            }
            last_pts = packet->pts;
        }
        
        // 发送包到解码器
        ret = avcodec_send_packet(codec_ctx_, packet);
        if (ret < 0) {
            av_packet_unref(packet);
            continue;
        }
        
        // 接收解码后的帧
        ret = avcodec_receive_frame(codec_ctx_, av_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(packet);
            continue;
        } else if (ret < 0) {
            std::cerr << "[ERROR] 解码失败" << std::endl;
            break;
        }
        
        // 转换为 BGR 格式
        sws_scale(sws_ctx, av_frame->data, av_frame->linesize, 0,
            codec_ctx_->height, rgb_frame->data, rgb_frame->linesize);
        
        // 创建 cv::Mat
        cv::Mat frame(codec_ctx_->height, codec_ctx_->width, CV_8UC3, 
            rgb_frame->data[0], rgb_frame->linesize[0]);
        
        // 推入队列 (防雪崩机制 FR-4)
        if (!queue_.push(frame.clone())) {
            std::cerr << "[WARNING] 队列已满，丢弃此帧 | 当前队列大小：" 
                      << queue_.size() << std::endl;
        }
        
        av_packet_unref(packet);
    }
    
    // 清理资源
    av_free(out_buffer);
    sws_freeContext(sws_ctx);
    av_frame_free(&rgb_frame);
    av_frame_free(&av_frame);
    av_packet_free(&packet);
}

} // namespace cybervision
