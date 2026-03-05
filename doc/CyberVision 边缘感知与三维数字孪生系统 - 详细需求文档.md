
# 软件需求规格说明书 (Software Requirements Specification)

**项目名称**: CyberVision Edge - 边缘端实时感知与三维数字孪生系统
**开发环境**: Ubuntu 22.04 LTS
**硬件约束**: 单节点运行，CPU >= 6核，**单张 RTX 4060 (8GB 显存) 封顶**
**核心技术栈**: C++17, ROS 2 (Humble), Qt 5/6, OpenSceneGraph, TensorRT (C++ API), FFmpeg (C API), OpenCV

---

## 1. 系统总体架构与约束

### 1.1 设计哲学：宿主机模拟边缘设备
为了模拟真实的自动驾驶域控制器（如 Orin-X）或工业机器人主控板，系统必须拆分为两个**物理上独立、通过 ROS 2 DDS 通信的进程**。
**绝对禁止**将推流、推理、渲染写在同一个可执行文件里。这种单体架构在面试官眼里是典型的“学生玩具”。

### 1.2 显存资源预算红线 (The 8GB VRAM Constraint)
由于你只有 8GB 显存，这是项目的**核心工程难点**。在你的设计里，你必须遵守以下预算分配：
*   **Ubuntu 桌面系统占用**: ~1.5 GB
*   **进程 A (AI 推理节点)**: ~3.0 GB (TensorRT 预分配、CUDA Context、显存流转)
*   **进程 B (OSG 渲染节点)**: ~2.5 GB (OpenGL 纹理、场景图缓冲)
*   **安全冗余**: 1.0 GB（防 OOM 崩溃区）

*此预算表请在面试时直接背出来，这是你具备系统级视野的铁证。*

---

## 2. 详细功能模块需求 (FR)

### 模块 A：`cybervision_infer_node` (感知与推理进程)

#### 2.A.1 FFmpeg 异步推流流水线 (Data Ingestion Pipeline)
*   **输入源**: 一段 1080P 或 4K 的行车记录仪 MP4 视频。
*   **功能需求**:
    *   **FR-1**: 封装 `VideoStreamer` 类，基于 FFmpeg `libavcodec` 软解视频帧。
    *   **FR-2**: 解码线程必须根据视频自带的 PTS (Presentation Time Stamp) 进行休眠控制，**严格以视频原有的帧率（如 30 FPS）拉取**。不允许全速解码导致队列瞬间爆满。
    *   **FR-3**: 解码后的帧转为 `cv::Mat` (BGR)，推入一个定长的线程安全队列（`ThreadSafeQueue`，深度最大设为 5 帧）。
    *   **FR-4 (防雪崩机制)**: 当队列满时，解码线程必须**主动丢弃新帧**，并输出 Warning 日志，绝不能阻塞或内存暴涨。

#### 2.A.2 TensorRT 异构计算引擎 (AI Inference Core)
*   **功能需求**:
    *   **FR-5**: 将 YOLOv8 的 `.onnx` 模型通过 TensorRT `trtexec` 或 C++ API 编译为序列化的 `.engine` 计划文件。
    *   **FR-6**: 封装 `TrtEngine` 类。**在类构造函数中**，预先调用 `cudaMalloc` 分配好输入和输出张量所需的固定大小显存空间。
    *   **FR-7 (零泄漏保障)**: 在推理的 `while(true)` 循环中，**严禁**出现任何 `new/delete` 或 `malloc/free`，必须复用预分配的内存。
    *   **FR-8**: 手工 C++ 实现 NMS（非极大值抑制）后处理算法，剔除重叠的 Bounding Box。

#### 2.A.3 坐标系反投影 (Inverse Perspective Mapping)
*   **功能需求**:
    *   **FR-9**: AI 输出的是 2D 像素坐标 $(u, v)$。假设一个虚拟的相机内参矩阵 $K$，并假设地平面 $Z=0$。
    *   **FR-10**: 编写 `CameraModel::projectTo3D` 方法，根据小孔成像原理，将二维目标的底边中心点映射为三维坐标 $(X, Y, 0)$，并估算目标的 3D 长宽高（可使用常量假设，如汽车长4m宽2m）。

#### 2.A.4 ROS 2 数据分发 (Middleware Publisher)
*   **功能需求**:
    *   **FR-11**: 定义自定义的 ROS 2 消息包 `cybervision_interfaces`。
    *   **FR-12**: 以 `SensorData QoS` 策略发布感知结果。包含当前系统时间戳、帧序号、包含所有 3D 边界框坐标的数组。

---

### 模块 B：`cybervision_render_node` (渲染与孪生进程)

#### 2.B.1 ROS 2 订阅与跨线程数据同步
*   **功能需求**:
    *   **FR-13**: 以后台独立线程运行 `rclcpp::spin` 接收感知数据。
    *   **FR-14 (无锁优化/双缓冲机制)**:
        *   定义结构体 `PerceptionFrame`。
        *   ROS 2 线程（写者）将最新数据写入 `shared_ptr<PerceptionFrame> buffer_A`。
        *   OSG 渲染线程（读者）从 `buffer_B` 读取。
        *   每一帧渲染开始前，通过极短的原子操作（或 `std::mutex` 交换指针）完成 A/B 缓冲区翻转。
        *   *绝对禁止在 ROS 2 回调函数里直接操作 Qt 或 OSG 的渲染树！*

#### 2.B.2 OpenSceneGraph 三维渲染管线
*   **功能需求**:
    *   **FR-15**: 继承 `osgQt::GraphicsWindowQt`（或直接使用 Qt 的 OpenGL widget），将 OSG 嵌入 QMainWindow。
    *   **FR-16**: 构建静态场景图：包含无限网格地面 (osg::Grid)、天空盒、相机坐标系原点。
    *   **FR-17 (对象池设计模式)**:
        *   由于动态 `new/delete` OSG 节点开销极大，必须预先在场景图中挂载 50 个隐藏的 `osg::Box`。
        *   每帧渲染时（`UpdateCallback`），根据传来的目标数量，激活对应的 Box，修改其 `osg::MatrixTransform` 实现平移，改变材质颜色（如人是红色，车是绿色），其余隐藏。

#### 2.B.3 Qt 交互大屏界面 (GUI)
*   **功能需求**:
    *   **FR-18**: UI 左侧播放实时的 2D 视频画面（利用 OpenCV `cv::imshow` 转 QImage 并在 QLabel 上绘制，顺便把检测框画上去），UI 右侧为 OSG 的 3D 俯视场景。
    *   **FR-19**: 增加性能监控面板（QCustomPlot 等），实时显示：
        *   AI 推理耗时 (毫秒)
        *   ROS 2 传输端到端延迟 (毫秒)
        *   OSG 渲染帧率 (FPS)

---

## 3. 非功能性需求 (NFR) - 性能与质量指标

要在面试中脱颖而出，这些指标必须实测达标：
1.  **端到端延迟 (End-to-End Latency)**：从视频帧读取，经过 AI，通过 ROS 2，直到 OSG 画出 3D 框，全流程延迟必须 **< 60 ms**。
2.  **吞吐量 (Throughput)**：在 4060 GPU 上，YOLOv8s 模型（FP16 量化），推理速度必须达到 **>= 60 FPS**（即推断时间 < 16ms）。
3.  **内存安全性 (Memory Safety)**：使用 `valgrind --leak-check=full` 或 CMake 加入 `-fsanitize=address` 编译，连续跑 2 小时，不得有任何内存或显存泄漏报错。
4.  **CPU 占用率**：渲染进程在限制了 60Hz 刷新率的前提下，单核 CPU 占用率不得超过 30%（展示你的休眠策略是否合理）。

---

## 4. 实施里程碑 (Implementation Roadmap)

不要一上来就写复杂的框架，按照这个顺序“拼乐高”：

### 第一周：C++ 与 AI 的亲密接触 (打通 TensorRT)
*   **任务**: 找一段网上的 TensorRT 推理 YOLOv8 的开源 C++ 代码（GitHub 大把）。
*   **目标**: 编译通过，给它喂一张带车和人的 JPG 图片，它能在控制台打印出精确的 $[X_1, Y_1, X_2, Y_2]$ 坐标。
*   **价值**: 跨越 AI 部署的新手村门槛。

### 第二周：你的舒适区 (FFmpeg 与多线程流水线)
*   **任务**: 写一个死循环的 `cv::VideoCapture` 或 FFmpeg，源源不断地把视频帧送给第一周写的 TensorRT 引擎。
*   **目标**: 看着控制台以极快的速度（>60FPS）疯狂刷出坐标。
*   **价值**: 掌握异构系统最基础的多线程同步与防 OOM（主动丢帧）策略。

### 第三周：ROS 2 桥接 (切断进程单体)
*   **任务**: 引入 Colcon 工作空间。把第二周的程序包装成 ROS 2 Publisher。另起一个终端运行写好的很简单 Subscriber 打印收到的坐标。
*   **目标**: 熟悉 `ament_cmake` 和 `msg` 自定义消息的编译。体验 DDS 底层通信。
*   **价值**: 从“写工具的”晋升为“懂系统架构的”。

### 第四周：OSG 与多线程死亡之舞 (前端渲染)
*   **任务**: 在 Subscriber 端引入 Qt 和 OSG。
*   **目标**: 解决 ROS 2 接收线程和 OSG 渲染主循环之间的锁冲突（双缓冲机制）。让 3D 框随着视频移动起来。
*   **价值**: 展现你对 C++ 并发编程和内存屏障的深刻理解。

### 第五周：反投影与工程化验收 (完善与打包)
*   **任务**: 加上 2D 到 3D 的反投影数学逻辑；完善 UI；写极其详细的 README。
*   **目标**: 拍一个 3 分钟的演示视频（录屏），上传 B 站，把链接放在简历里。
