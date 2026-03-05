# 感知结果消息定义
# 包含一帧中所有检测到的 3D 目标

builtin_interfaces/Time timestamp     # 时间戳
int64 frame_id                        # 帧序号
DetectedObject3D[] objects            # 检测到的目标数组
float32 inference_time_ms             # AI 推理耗时 (毫秒)
string camera_frame                   # 相机坐标系标识
