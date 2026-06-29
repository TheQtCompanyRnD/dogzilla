# Copyright (C) 2026 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#
# Onboard 2D SLAM stack for the Dogzilla S2, meant to run on the Pi 5 alongside
# dogzillad (which publishes the LaserScan and the base_link->laser_frame TF).
#
#   dogzillad --/dogzilla/sensor_msgs/msg/LaserScan--> rf2o_laser_odometry
#                                                         |  (odom + odom->base_link TF)
#                                                         v
#                                                      slam_toolbox --> /map + map->odom
#
# rf2o derives odometry purely from successive laser scans (no wheel encoders,
# which a legged robot doesn't have, and the firmware yaw drifts ~14 deg/s).
#
# Usage:  ros2 launch slam.launch.py
# Prereq: sudo apt install ros-$ROS_DISTRO-slam-toolbox ros-$ROS_DISTRO-rf2o-laser-odometry

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


SCAN_TOPIC = '/dogzilla/sensor_msgs/msg/LaserScan'


def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time')
    params_file = LaunchConfiguration('slam_params_file')

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument(
            'slam_params_file',
            default_value=os.path.join(
                os.path.dirname(os.path.realpath(__file__)),
                'slam_toolbox.yaml'),
            description='slam_toolbox configuration'),

        # Laser-scan-matching odometry -> publishes /odom and the odom->base_link TF.
        Node(
            package='rf2o_laser_odometry',
            executable='rf2o_laser_odometry_node',
            name='rf2o_laser_odometry',
            output='screen',
            parameters=[{
                'laser_scan_topic': SCAN_TOPIC,
                'odom_topic': '/odom',
                'publish_tf': True,
                'base_frame_id': 'base_link',
                'odom_frame_id': 'odom',
                'laser_frame_id': 'laser_frame',
                'init_pose_from_topic': '',   # start at the origin
                'freq': 10.0,                 # MS200 spins ~10 Hz
                'use_sim_time': use_sim_time,
            }],
        ),

        # 2D graph SLAM -> builds /map and publishes the map->odom TF.
        Node(
            package='slam_toolbox',
            executable='async_slam_toolbox_node',
            name='slam_toolbox',
            output='screen',
            parameters=[params_file, {'use_sim_time': use_sim_time}],
        ),
    ])
