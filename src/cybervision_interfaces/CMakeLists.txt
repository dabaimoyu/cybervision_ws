cmake_minimum_required(VERSION 3.14)
project(cybervision_interfaces VERSION 0.1.0 LANGUAGES C CXX)

# 找到 ROS 2 依赖包
find_package(ament_cmake REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)

# 定义自定义消息文件
set(msg_files
  "msg/PerceptionResult.msg"
  "msg/DetectedObject3D.msg"
)

# 使用 rosidl 生成消息代码
rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  DEPENDENCIES std_msgs geometry_msgs builtin_interfaces
)

# 导出依赖
ament_export_dependencies(rosidl_default_runtime)

# 安装消息文件
install(
  DIRECTORY msg
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
