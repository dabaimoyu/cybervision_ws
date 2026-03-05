#!/usr/bin/env python3
"""
CyberVision Edge 系统启动脚本
同时启动推理节点和渲染节点
"""

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    # 声明参数
    video_path_arg = DeclareLaunchArgument(
        'video_path',
        default_value='',
        description='输入视频文件路径'
    )
    
    engine_path_arg = DeclareLaunchArgument(
        'engine_path',
        default_value='',
        description='TensorRT engine 文件路径'
    )
    
    # 推理节点 (进程 A)
    infer_node = Node(
        package='cybervision_edge',
        executable='cybervision_infer_node',
        name='cybervision_infer_node',
        output='screen',
        parameters=[{
            'video_path': LaunchConfiguration('video_path'),
            'engine_path': LaunchConfiguration('engine_path')
        }]
    )
    
    # 渲染节点 (进程 B)
    render_node = Node(
        package='cybervision_edge',
        executable='cybervision_render_node',
        name='cybervision_render_node',
        output='screen'
    )
    
    return LaunchDescription([
        video_path_arg,
        engine_path_arg,
        infer_node,
        render_node
    ])
