# CyberVision Edge 编译命令完整指南

## 📋 文档信息

- **项目名称**: CyberVision Edge - 边缘端实时感知与三维数字孪生系统
- **文档版本**: 1.0
- **创建日期**: 2026 年 3 月 4 日
- **适用环境**: Ubuntu 22.04 LTS + ROS 2 Humble
- **TensorRT 版本**: 10.15.1 (CUDA 13.1)

---

## 🎯 前置条件检查

### 1. 系统依赖安装

```bash
# 确认 ROS 2 Humble 已安装
source /opt/ros/humble/setup.bash
ros2 --version

# 确认 TensorRT 已安装
dpkg -l | grep tensorrt
# 应显示：tensorrt 10.15.1.29-1+cuda13.1

# 确认 FFmpeg 开发库已安装
pkg-config --modversion libavcodec
pkg-config --modversion libavformat

# 确认 OpenCV 已安装
pkg-config --modversion opencv4
```

### 2. 环境变量设置

```bash
# 永久添加 ROS 2 环境变量（只需执行一次）
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc

# 临时设置（每次终端会话需要执行）
source /opt/ros/humble/setup.bash
```

---

## 📦 项目结构

```
cybervision_ws/
├── src/
│   ├── cybervision_interfaces/    # ROS 2 消息接口包
│   │   ├── msg/                   # 自定义消息定义
│   │   │   ├── PerceptionResult.msg
│   │   │   └── DetectedObject3D.msg
│   │   ├── CMakeLists.txt
│   │   └── package.xml
│   │
│   └── cybervision_edge/          # 主功能包
│       ├── include/               # 头文件
│       │   ├── video_streamer.hpp
│       │   ├── trt_engine.hpp
│       │   ├── camera_model.hpp
│       │   ├── performance_monitor.hpp
│       │   ├── gpu_memory_monitor.hpp
│       │   ├── double_buffer.hpp
│       │   └── qos_profiles.hpp
│       │
│       ├── src/
│       │   ├── infer_node/        # AI 推理节点源码
│       │   │   ├── main.cpp
│       │   │   ├── video_streamer.cpp
│       │   │   ├── trt_engine.cpp
│       │   │   ├── camera_model.cpp
│       │   │   ├── performance_monitor.cpp
│       │   │   ├── gpu_memory_monitor.cpp
│       │   │   └── test_node.cpp      # 测试节点（简化版）
│       │   │
│       │   └── render_node/       # 渲染节点源码
│       │       ├── main.cpp
│       │       ├── perception_subscriber.cpp
│       │       ├── osg_renderer.cpp
│       │       ├── qt_gui.cpp
│       │       └── double_buffer.cpp
│       │
│       ├── CMakeLists.txt         # CMake 构建配置
│       └── package.xml            # ROS 2 包描述
│
├── build/                         # 编译输出目录
└── install/                       # 安装目录
```

---

## 🔨 编译步骤详解

### 步骤 1: 编译消息接口包（必须首先编译）

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws

# 清理之前的编译缓存（可选，遇到奇怪错误时执行）
rm -rf build/ install/ log/

# 编译 cybervision_interfaces 包
colcon build --packages-select cybervision_interfaces \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release \
  --parallel-workers $(nproc)

# 验证编译结果
ls -lh install/cybervision_interfaces/share/cybervision_interfaces/msg/
# 应看到：PerceptionResult.msg, DetectedObject3D.msg
```

**参数说明**:
- `--packages-select cybervision_interfaces`: 仅编译指定包
- `--cmake-args=-DCMAKE_BUILD_TYPE=Release`: 启用 Release 优化
- `--parallel-workers $(nproc)`: 使用所有 CPU 核心并行编译

**预期输出**:
```
Starting >>> cybervision_interfaces
Finished <<< cybervision_interfaces [3.5s]

Summary: 1 package finished [3.8s]
```

---

### 步骤 2: 编译主功能包（包含所有节点）

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws

# 刷新环境变量（包含刚编译的消息包）
source install/setup.bash

# 编译 cybervision_edge 包
colcon build --packages-select cybervision_edge \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release \
  --parallel-workers $(nproc)

# 验证编译结果
ls -lh install/cybervision_edge/lib/cybervision_edge/
# 应看到三个可执行文件：
# - cybervision_infer_node         (约 1.3MB)
# - cybervision_infer_test_node    (约 0.8MB)
# - cybervision_render_node        (约 604KB)
```

**注意**: 此步骤会自动编译以下组件：
- ✅ AI 推理节点（完整功能，需要实际模型和视频）
- ✅ 测试推理节点（简化版，无需模型和视频）
- ✅ 渲染节点（控制台版本，OSG+Qt 待开发）

**预期输出**:
```
Starting >>> cybervision_edge
Finished <<< cybervision_edge [8.2s]

Summary: 1 package finished [8.5s]
```

---

### 步骤 3: 分步编译单个节点（可选，用于调试）

#### 仅编译 AI 推理节点（完整版）

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash

# 修改 CMakeLists.txt，注释掉其他节点，只保留 cybervision_infer_node
# 然后执行：
colcon build --packages-select cybervision_edge \
  --cmake-targets cybervision_infer_node \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release
```

#### 仅编译测试节点

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash

colcon build --packages-select cybervision_edge \
  --cmake-targets cybervision_infer_test_node \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release
```

#### 仅编译渲染节点

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash

colcon build --packages-select cybervision_edge \
  --cmake-targets cybervision_render_node \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release
```

---

### 步骤 4: 清理并重新编译（遇到问题时）

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws

# 完全清理
rm -rf build/ install/ log/

# 重新编译所有包
source /opt/ros/humble/setup.bash
colcon build --cmake-args=-DCMAKE_BUILD_TYPE=Release \
  --parallel-workers $(nproc)

# 或者分步编译
colcon build --packages-select cybervision_interfaces \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release

source install/setup.bash

colcon build --packages-select cybervision_edge \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release
```

---

## 🚀 运行测试命令

### 测试 1: 启动 AI 推理节点（测试模式）

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash

# 启动测试节点（无需实际模型和视频）
ros2 run cybervision_edge cybervision_infer_test_node

# 预期输出：
# [INFO] CyberVision Edge - AI 推理节点 (测试模式)
# [INFO] QoS 策略：BestEffort + Volatile
# [INFO] 目标 FPS: 60
# [INFO] ╔════════════════════════════════════════╗
# [INFO] ║   Performance Report (Frame     30)    ║
# [INFO] ║ FPS: 62.6 ✅ (Target: ≥60)
# [INFO] ║ End-to-End Latency: 12.45ms ✅ (Target: <60ms)
```

### 测试 2: 启动 AI 推理节点（完整功能）

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash

# 需要实际的模型和视频文件
ros2 run cybervision_edge cybervision_infer_node \
  --ros-args \
  -p video_path:=/path/to/your/video.mp4 \
  -p engine_path:=/path/to/your/model.engine
```

### 测试 3: 启动渲染节点

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash

# 启动渲染节点（控制台版本）
ros2 run cybervision_edge cybervision_render_node

# 预期输出：
# [INFO] [cybervision_render_node]: CyberVision 渲染节点初始化完成!
```

### 测试 4: 双节点同时运行（验证 DDS 通信）

**终端 1 - 启动推理节点**:
```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash
ros2 run cybervision_edge cybervision_infer_test_node
```

**终端 2 - 启动渲染节点**:
```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash
ros2 run cybervision_edge cybervision_render_node
```

**终端 3 - 查看话题通信**:
```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source install/setup.bash

# 查看话题列表
ros2 topic list
# 应看到：/perception_results

# 查看话题类型
ros2 topic type /perception_results
# 应显示：cybervision_interfaces/msg/PerceptionResult

# 查看消息频率
ros2 topic hz /perception_results
# 应显示：average rate: 62.6Hz

# 查看消息内容
ros2 topic echo /perception_results --once
```

---

## ⚠️ 常见编译错误与解决方案

### 错误 1: package.xml 格式错误

**错误信息**:
```
The manifest contains invalid XML: syntax error: line 1, column 0
```

**解决方案**:
```bash
# 确保 package.xml 使用 XML 格式而非 YAML
head -5 src/cybervision_interfaces/package.xml
# 应以 <?xml version="1.0"?> 开头
```

---

### 错误 2: TensorRT 头文件缺失

**错误信息**:
```
fatal error: NvInfer.h: No such file or directory
```

**解决方案**:
```bash
# 确认 TensorRT 已正确安装
dpkg -l | grep tensorrt

# 查找头文件位置
find /usr -name "NvInfer.h"

# 如果找不到，需要重新安装 TensorRT
wget https://developer.nvidia.com/downloads/...
sudo dpkg -i nv-tensorrt-local-repo-*.deb
sudo apt-get update
sudo apt-get install -y tensorrt
```

---

### 错误 3: FFmpeg 库链接失败

**错误信息**:
```
undefined reference to `avcodec_free_context'
```

**解决方案**:
```bash
# 确认 FFmpeg 开发库已安装
pkg-config --libs libavcodec
pkg-config --libs libavformat

# 如果未找到，安装开发库
sudo apt-get install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev

# 清理并重新编译
rm -rf build/ install/
colcon build --packages-select cybervision_edge
```

---

### 错误 4: ROS 2 消息字段不匹配

**错误信息**:
```
error: 'struct cybervision_interfaces::msg::PerceptionResult_' has no member named 'stamp'
```

**解决方案**:
```bash
# 查看实际的消息定义
cat src/cybervision_interfaces/msg/PerceptionResult.msg

# 应使用 timestamp 而非 stamp
# 修改代码：
# ❌ perception_msg->stamp = this->now();
# ✅ perception_msg->timestamp = this->now();
```

---

### 错误 5: 依赖包未编译

**错误信息**:
```
Package 'cybervision_interfaces' not found
```

**解决方案**:
```bash
# 必须先编译消息接口包
colcon build --packages-select cybervision_interfaces

# 刷新环境变量
source install/setup.bash

# 然后再编译主功能包
colcon build --packages-select cybervision_edge
```

---

## 🛠️ 高级编译选项

### Debug 模式编译

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source /opt/ros/humble/setup.bash

colcon build --cmake-args=-DCMAKE_BUILD_TYPE=Debug \
  --parallel-workers $(nproc)
```

**用途**: 启用调试符号，配合 gdb 调试

---

### 带性能分析的编译

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source /opt/ros/humble/setup.bash

colcon build --cmake-args=-DCMAKE_BUILD_TYPE=RelWithDebInfo \
  --parallel-workers $(nproc)
```

**用途**: Release 优化 + 调试符号，用于性能分析

---

### 静态分析编译

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source /opt/ros/humble/setup.bash

# 使用 clang-tidy 进行静态分析
colcon build --cmake-args=-DCMAKE_CXX_CLANG_TIDY=clang-tidy \
  --parallel-workers $(nproc)
```

---

### 生成编译数据库

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws
source /opt/ros/humble/setup.bash

colcon build --cmake-args=-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  --parallel-workers $(nproc)

# 生成 compile_commands.json，供 IDE 使用
ln -s build/compile_commands.json .
```

---

## 📊 编译性能优化

### 1. 并行编译

```bash
# 使用所有 CPU 核心
colcon build --parallel-workers $(nproc)

# 或指定核心数（例如 4 核）
colcon build --parallel-workers 4
```

### 2. 增量编译

```bash
# 仅编译修改过的文件
colcon build --packages-select cybervision_edge

# colcon 会自动检测变更的文件并增量编译
```

### 3. 缓存优化

```bash
# 使用 ccache 加速重复编译
sudo apt-get install ccache

# 在 ~/.bashrc 中添加
export CC=/usr/lib/ccache/gcc
export CXX=/usr/lib/ccache/g++

# 重新编译
colcon build
```

---

## 🎯 完整测试流程脚本

### 一键编译并测试脚本

创建 `test_build.sh`:

```bash
#!/bin/bash

set -e  # 遇到错误立即退出

echo "=========================================="
echo "CyberVision Edge 编译与测试脚本"
echo "=========================================="

# 进入工作目录
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws

# 清理（可选）
if [ "$1" == "--clean" ]; then
    echo "清理旧的编译文件..."
    rm -rf build/ install/ log/
fi

# 设置环境
echo "加载 ROS 2 环境..."
source /opt/ros/humble/setup.bash

# 编译消息包
echo ""
echo "步骤 1: 编译 cybervision_interfaces..."
colcon build --packages-select cybervision_interfaces \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release \
  --parallel-workers $(nproc)

# 刷新环境
source install/setup.bash

# 编译主功能包
echo ""
echo "步骤 2: 编译 cybervision_edge..."
colcon build --packages-select cybervision_edge \
  --cmake-args=-DCMAKE_BUILD_TYPE=Release \
  --parallel-workers $(nproc)

# 验证编译结果
echo ""
echo "步骤 3: 验证编译结果..."
ls -lh install/cybervision_edge/lib/cybervision_edge/

echo ""
echo "=========================================="
echo "✅ 编译成功！"
echo "=========================================="
echo ""
echo "运行测试:"
echo "  终端 1: ros2 run cybervision_edge cybervision_infer_test_node"
echo "  终端 2: ros2 run cybervision_edge cybervision_render_node"
echo "  终端 3: ros2 topic hz /perception_results"
echo ""
```

使用方法:
```bash
chmod +x test_build.sh
./test_build.sh           # 增量编译
./test_build.sh --clean   # 清理后重新编译
```

---

## 📝 每日开发编译流程

### 早上开始工作

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws

# 拉取最新代码（如果使用 git）
git pull

# 加载环境
source /opt/ros/humble/setup.bash
source install/setup.bash 2>/dev/null || true

# 增量编译
colcon build --packages-select cybervision_edge \
  --parallel-workers $(nproc)

# 运行测试
source install/setup.bash
ros2 run cybervision_edge cybervision_infer_test_node
```

### 晚上提交代码前

```bash
cd /home/hp/opencv_workspace/vsg_demo/test/cybervision_ws

# 完全清理并重新编译
rm -rf build/ install/ log/
source /opt/ros/humble/setup.bash
colcon build --cmake-args=-DCMAKE_BUILD_TYPE=Release

# 运行所有测试
source install/setup.bash
# ... 运行各项测试 ...

# 提交代码
git add .
git commit -m "描述你的改动"
git push
```

---

## 🔗 相关文档

- [TensorRT 10.x 适配总结](./TensorRT10_Migration_Summary.md)
- [性能测试报告](./Performance_Test_Report.md)
- [ROS 2 消息接口定义](../src/cybervision_interfaces/msg/)

---

## 📞 快速参考卡片

```
╔══════════════════════════════════════════════════════════╗
║              CyberVision Edge 编译速查表                  ║
╠══════════════════════════════════════════════════════════╣
║ 1. 消息包编译                                             ║
║    colcon build --packages-select cybervision_interfaces ║
║                                                          ║
║ 2. 主功能包编译                                           ║
║    source install/setup.bash                             ║
║    colcon build --packages-select cybervision_edge       ║
║                                                          ║
║ 3. 测试推理节点                                           ║
║    ros2 run cybervision_edge cybervision_infer_test_node ║
║                                                          ║
║ 4. 渲染节点                                               ║
║    ros2 run cybervision_edge cybervision_render_node     ║
║                                                          ║
║ 5. 查看话题                                               ║
║    ros2 topic hz /perception_results                     ║
╚══════════════════════════════════════════════════════════╝
```

---

**文档更新时间**: 2026 年 3 月 4 日  
**维护者**: CyberVision Edge Team  
**反馈**: 如有问题请提交 Issue
