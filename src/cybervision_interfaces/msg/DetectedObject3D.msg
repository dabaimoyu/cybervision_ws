# 3D 检测目标消息定义
# 用于描述单个检测到的三维目标

float32 x           # X 坐标 (世界坐标系，单位：米)
float32 y           # Y 坐标 (世界坐标系，单位：米)
float32 z           # Z 坐标 (世界坐标系，单位：米，通常为 0 表示地平面)
float32 length      # 目标长度 (单位：米)
float32 width       # 目标宽度 (单位：米)
float32 height      # 目标高度 (单位：米)
string object_type  # 目标类型："car", "pedestrian", "truck", "bus"等
float32 confidence  # 置信度 [0.0, 1.0]
int32 track_id      # 跟踪 ID
