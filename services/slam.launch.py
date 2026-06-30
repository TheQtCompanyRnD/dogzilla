# Copyright (C) 2026 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#
# Onboard 2D SLAM for the Dogzilla S2, meant to run on the Pi 5 alongside
# dogzillad (which publishes the LaserScan and the base_link->laser_frame TF).
#
#   dogzillad --/dogzilla/sensor_msgs/msg/LaserScan (remapped to scan)--> cartographer_node
#                                                                            |
#                                          map->odom->base_link TF + submaps |
#                                                                            v
#                                              cartographer_occupancy_grid_node --> /map
#
# Cartographer's 2D local SLAM scan-matches internally, so NO external
# odometry node is needed -- a good fit for a legged robot with no wheel
# encoders and a drifting firmware yaw. It uses only the LaserScan plus the
# static base_link->laser_frame transform, and publishes the full
# map->odom->base_link TF chain itself (provide_odom_frame=true).
#
# IMU is disabled (use_imu_data=false in dogzilla_2d.lua): dogzillad currently
# publishes attitude as PoseStamped on /dogzilla/body_pose/state, not a
# sensor_msgs/Imu, so there is nothing for Cartographer to consume yet. To fuse
# IMU later (helps with the gait rocking the lidar), publish a sensor_msgs/Imu
# and set use_imu_data=true with tracking_frame set to the IMU frame.
#
# Usage:  ros2 launch slam.launch.py
# Prereq: sudo apt install ros-$ROS_DISTRO-cartographer-ros

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


SCAN_TOPIC = '/dogzilla/sensor_msgs/msg/LaserScan'
CONFIG_DIR = os.path.dirname(os.path.realpath(__file__))


def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time')
    resolution = LaunchConfiguration('resolution')
    publish_period_sec = LaunchConfiguration('publish_period_sec')

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('resolution', default_value='0.05'),
        DeclareLaunchArgument('publish_period_sec', default_value='1.0'),

        # Local + global 2D SLAM. Publishes map->odom->base_link and submaps.
        Node(
            package='cartographer_ros',
            executable='cartographer_node',
            name='cartographer_node',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time}],
            arguments=[
                '-configuration_directory', CONFIG_DIR,
                '-configuration_basename', 'dogzilla_2d.lua',
            ],
            remappings=[('scan', SCAN_TOPIC)],
        ),

        # Rasterizes the submaps into a nav_msgs/OccupancyGrid on /map.
        Node(
            package='cartographer_ros',
            executable='cartographer_occupancy_grid_node',
            name='cartographer_occupancy_grid_node',
            output='screen',
            parameters=[{
                'use_sim_time': use_sim_time,
                'resolution': resolution,
                'publish_period_sec': publish_period_sec,
            }],
        ),
    ])
