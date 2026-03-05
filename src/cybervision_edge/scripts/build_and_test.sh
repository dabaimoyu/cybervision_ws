#!/bin/bash

# CyberVision Edge 编译与测试脚本

set -e  # 遇到错误立即退出

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKSPACE_DIR="$SCRIPT_DIR/.."

echo "======================================"
echo "CyberVision Edge 编译与测试"
echo "======================================"
echo ""

# 1. 检查依赖
echo "[1/4] 检查 ROS 2 环境..."
if [ ! -f "/opt/ros/humble/setup.bash" ]; then
    echo "❌ 错误：未检测到 ROS 2 Humble"
    echo "请先安装：sudo apt install ros-humble-desktop-full"
    exit 1
fi
echo "✅ ROS 2 Humble 已安装"

# 2. 加载 ROS 2 环境
echo ""
echo "[2/4] 加载 ROS 2 环境..."
source /opt/ros/humble/setup.bash
cd "$WORKSPACE_DIR"

# 3. 安装依赖
echo ""
echo "[3/4] 安装 ROS 2 依赖..."
rosdep update
rosdep install --from-paths src --ignore-src -r -y --skip-keys libopenscenegraph-dev || true

# 4. 编译
echo ""
echo "[4/4] 编译项目..."
colcon build --symlink-install --cmake-args=-DCMAKE_BUILD_TYPE=Release --packages-select cybervision_interfaces cybervision_edge

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "✅ 编译成功!"
    echo "======================================"
    echo ""
    echo "下一步操作:"
    echo "1. 加载环境变量："
    echo "   source install/setup.bash"
    echo ""
    echo "2. 准备 YOLOv8 模型并转换为 TensorRT:"
    echo "   cd scripts"
    echo "   bash build_engine.sh ../models/yolov8s.onnx ../models/yolov8s.engine true"
    echo ""
    echo "3. 运行系统:"
    echo "   ros2 launch cybervision_edge cybervision.launch.py \\"
    echo "       video_path:=/path/to/video.mp4 \\"
    echo "       engine_path:=/path/to/yolov8s.engine"
    echo ""
else
    echo ""
    echo "======================================"
    echo "❌ 编译失败!"
    echo "======================================"
    echo ""
    echo "请检查以下常见问题:"
    echo "1. TensorRT 是否正确安装？"
    echo "2. CUDA 版本是否匹配？"
    echo "3. FFmpeg 开发库是否安装？"
    echo "4. OpenCV 是否正确配置？"
    echo ""
    exit 1
fi
