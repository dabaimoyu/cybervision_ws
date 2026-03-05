cmake_minimum_required(VERSION 3.14)
project(cybervision_edge VERSION 0.1.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 找到 ROS 2 依赖包
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(cybervision_interfaces REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(sensor_msgs REQUIRED)

# OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)

# TensorRT (需要手动指定路径)
set(TENSORRT_INCLUDE_DIR "/usr/include/x86_64-linux-gnu" CACHE PATH "TensorRT include directory")
set(TENSORRT_LIBRARY_DIR "/usr/lib/x86_64-linux-gnu" CACHE PATH "TensorRT library directory")

# CUDA
find_package(CUDA REQUIRED)

# FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG libavcodec libavformat libavutil swscale)
if(NOT FFMPEG_FOUND)
    message(WARNING "FFmpeg not found, video streaming will be disabled")
endif()

# Qt5
find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL REQUIRED)

# OpenSceneGraph
find_package(OpenSceneGraph COMPONENTS osg osgViewer osgUtil osgGA)
if(NOT OPENSCENEGRAPH_FOUND)
    message(WARNING "OpenSceneGraph not found, rendering will be disabled")
endif()

# 包含目录
include_directories(
  include
  ${OpenCV_INCLUDE_DIRS}
  ${TENSORRT_INCLUDE_DIR}
  ${CUDA_INCLUDE_DIRS}
  ${FFMPEG_INCLUDE_DIRS}
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${Qt5OpenGL_INCLUDE_DIRS}
  ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# 进程 A: AI 推理节点
add_executable(cybervision_infer_node
  src/infer_node/main.cpp
  src/infer_node/video_streamer.cpp
  src/infer_node/trt_engine.cpp
  src/infer_node/camera_model.cpp
  src/infer_node/performance_monitor.cpp
  src/infer_node/gpu_memory_monitor.cpp
)

# 测试用推理节点（不需要实际模型和视频）
add_executable(cybervision_infer_test_node
  src/infer_node/test_node.cpp
  src/infer_node/performance_monitor.cpp
)

target_link_libraries(cybervision_infer_test_node
  ${OpenCV_LIBRARIES}
)

ament_target_dependencies(cybervision_infer_test_node
  rclcpp
  std_msgs
  geometry_msgs
  cybervision_interfaces
  cv_bridge
  sensor_msgs
)

target_link_libraries(cybervision_infer_node
  ${OpenCV_LIBRARIES}
  ${TENSORRT_LIBRARY_DIR}/libnvinfer.so
  ${TENSORRT_LIBRARY_DIR}/libnvonnxparser.so
  ${CUDA_LIBRARIES}
  avcodec
  avformat
  avutil
  swscale
)

ament_target_dependencies(cybervision_infer_node
  rclcpp
  std_msgs
  geometry_msgs
  cybervision_interfaces
  cv_bridge
  sensor_msgs
)

# 进程 B: 渲染节点
add_executable(cybervision_render_node
  src/render_node/main.cpp
  src/render_node/perception_subscriber.cpp
  src/render_node/osg_renderer.cpp
  src/render_node/qt_gui.cpp
  src/render_node/double_buffer.cpp
)

# Qt5 MOC 处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

target_link_libraries(cybervision_render_node
  ${OpenCV_LIBRARIES}
  ${Qt5Core_LIBRARIES}
  ${Qt5Gui_LIBRARIES}
  ${Qt5Widgets_LIBRARIES}
  ${Qt5OpenGL_LIBRARIES}
  ${OPENSCENEGRAPH_LIBRARIES}
  osgText
)

# DDS 延迟测试工具（暂时禁用）
# add_executable(dds_latency_test
#   tools/dds_latency_test.cpp
# )

ament_target_dependencies(cybervision_render_node
  rclcpp
  std_msgs
  geometry_msgs
  cybervision_interfaces
)

# 安装规则
install(TARGETS
  cybervision_infer_node
  cybervision_infer_test_node
  cybervision_render_node
  DESTINATION lib/${PROJECT_NAME})

# 导出依赖
ament_export_include_directories(include)
ament_export_libraries(cybervision_infer_node cybervision_render_node)
ament_export_dependencies(rclcpp std_msgs geometry_msgs cybervision_interfaces cv_bridge sensor_msgs)

ament_package()
