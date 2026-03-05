# CyberVision Edge 快速开始指南

## 环境要求

### 硬件要求
- CPU: >= 6 核
- GPU: NVIDIA RTX 4060 (8GB 显存) 或更高
- 内存：>= 16GB

### 软件依赖
- Ubuntu 22.04 LTS
- ROS 2 Humble Hawksbill
- CUDA 11.x / 12.x
- TensorRT 8.x
- OpenCV 4.x
- FFmpeg 4.x
- Qt5/Qt6
- OpenSceneGraph 3.6+

## 安装步骤

### 1. 安装基础依赖

```bash
# 安装 ROS 2 Humble
sudo apt update && sudo apt install -y ros-humble-desktop-full
source /opt/ros/humble/setup.bash

# 安装 OpenCV
sudo apt install -y libopencv-dev python3-opencv

# 安装 TensorRT (需要从 NVIDIA 官网下载或使用 SDK)
# 或者使用 apt 安装
sudo apt install -y tensorrt libnvinfer-dev

# 安装 FFmpeg
sudo apt install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev

# 安装 Qt5
sudo apt install -y qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev

# 安装 OpenSceneGraph
sudo apt install -y libopenscenegraph-dev

# 安装其他工具
sudo apt install -y libeigen3-dev libgoogle-glog-dev libgflags-dev
```

### 2. 编译项目

```bash
cd ~/cybervision_ws

# 安装 ROS 2 依赖
rosdep install --from-paths src --ignore-src -r -y

# 编译
colcon build --symlink-install --cmake-args=-DCMAKE_BUILD_TYPE=Release

# 加载环境变量
source install/setup.bash
```

### 3. 准备 YOLOv8 模型

#### 3.1 导出 ONNX 模型

```python
# export_yolov8.py
from ultralytics import YOLO

model = YOLO('yolov8s.pt')
model.export(format='onnx', imgsz=640)
```

#### 3.2 转换为 TensorRT Engine

```bash
cd ~/cybervision_ws/src/cybervision_edge/scripts

# 执行转换脚本
bash build_engine.sh ../models/yolov8s.onnx ../models/yolov8s.engine true
```

### 4. 运行系统

#### 方法一：使用 launch 文件启动（推荐）

```bash
cd ~/cybervision_ws
source install/setup.bash

ros2 launch cybervision_edge cybervision.launch.py \
    video_path:=/path/to/your/video.mp4 \
    engine_path:=/path/to/yolov8s.engine
```

#### 方法二：分别启动两个进程

终端 1 - 启动推理节点：
```bash
cd ~/cybervision_ws
source install/setup.bash

ros2 run cybervision_edge cybervision_infer_node \
    --ros-args \
    -p video_path:=/path/to/your/video.mp4 \
    -p engine_path:=/path/to/yolov8s.engine
```

终端 2 - 启动渲染节点：
```bash
cd ~/cybervision_ws
source install/setup.bash

ros2 run cybervision_edge cybervision_render_node
```

## 性能监控

运行时应该观察到以下指标：

### 预期性能指标
- **推理速度**: YOLOv8s FP16 >= 60 FPS (< 16ms)
- **端到端延迟**: < 60 ms
- **显存占用**: 
  - AI 推理节点：~3.0 GB
  - OSG 渲染节点：~2.5 GB
  - Ubuntu 桌面：~1.5 GB
  - 总计：< 7.0 GB (留 1GB 冗余)
- **CPU 占用率**: 单核 < 30% (在 60Hz 刷新率限制下)

### 调试输出示例

```
[INFO] [cybervision_infer_node]: 初始化 VideoStreamer...
[INFO] [cybervision_infer_node]: 视频 FPS: 30.0
[INFO] [cybervision_infer_node]: 加载 TensorRT 引擎...
[INFO] [cybervision_infer_node]: TensorRT 引擎加载成功!
[INFO] [cybervision_infer_node]: 输入尺寸：640x640
[INFO] [cybervision_infer_node]: 输入显存：4 MB
[INFO] [cybervision_infer_node]: 输出显存：2 MB
[INFO] [cybervision_infer_node]: 相机模型初始化完成
[INFO] [cybervision_infer_node]: CyberVision 推理节点初始化完成!
帧率：90 | 检测目标数：5 | 推理耗时：12.5ms | 总耗时：28ms
```

## 常见问题排查

### Q1: TensorRT 引擎加载失败
**A:** 检查以下几点：
1. 确认 TensorRT 版本与 CUDA 版本匹配
2. 确认 `.engine` 文件是在相同 GPU 架构上生成的
3. 重新生成 engine 文件时清除旧的缓存

### Q2: 显存不足 (OOM)
**A:** 
1. 降低输入视频分辨率
2. 使用更小的 YOLOv8 模型 (如 yolov8n)
3. 确保启用了 FP16 模式

### Q3: ROS 2 节点间通信延迟高
**A:**
1. 检查 DDS QoS 策略配置
2. 减少消息发布频率
3. 使用 `rmw_fastrtps_cpp` 代替默认的 RMW 实现

### Q4: 渲染卡顿
**A:**
1. 检查双缓冲机制是否正确实现
2. 确保 OSG 场景图没有频繁的 new/delete 操作
3. 使用对象池模式预分配节点

## 下一步开发

- 

## 资源链接

- [TensorRT 官方文档](https://docs.nvidia.com/deeplearning/tensorrt/)
- [ROS 2 Humble 文档](https://docs.ros.org/en/humble/)
- [OpenSceneGraph 文档](http://www.openscenegraph.com/)
- [YOLOv8 文档](https://docs.ultralytics.com/)
