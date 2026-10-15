# Copyright (C) 2026 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#
# Onboard 2D SLAM (slam_toolbox backend) for the Dogzilla S2: an alternative to
# cartographer.launch.py, with the same inputs and outputs, so the twin and
# dogzillad don't care which one runs.
#
#   dogzillad --/dogzilla/sensor_msgs/msg/LaserScan--> slam_toolbox (async, mapping)
#                                                          |
#                                        map->odom TF + /dogzilla/map
#
# Unlike Cartographer, slam_toolbox only publishes map->odom and expects an
# odometry source to supply odom->base_link. The dog has none (no wheel
# encoders, drifting firmware yaw), so this launch publishes an IDENTITY
# odom->base_link and lets scan matching do all the work: map->odom then carries
# the whole pose. Consequences, all handled in slam_toolbox.yaml:
#   - the "travel since last scan" gates (minimum_travel_distance/heading) would
#     never open, so they are zeroed and scans are rate-limited by time instead;
#   - the scan matcher gets no motion prior, so its search window has to cover
#     the motion between processed scans.
# Better: odometry:=icp swaps the identity transform for rtabmap_odom's
# icp_odometry, which scan-matches successive LaserScans (point-to-point ICP,
# 3DoF) and publishes odom->base_link itself, so slam_toolbox gets a real motion
# prior. It reads only the same LaserScan + base_link->laser_frame TF. (The
# slam_toolbox.yaml travel gates stay zeroed; they are harmless with real odom.)
#
# Usage:  ros2 launch slam_toolbox.launch.py [odometry:=identity|icp]

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, EmitEvent, LogInfo, RegisterEventHandler
from launch.conditions import LaunchConfigurationEquals
from launch.events import matches_action
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode, Node
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition


SCAN_TOPIC = '/dogzilla/sensor_msgs/msg/LaserScan'
CONFIG_FILE = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'slam_toolbox.yaml')

# Same namespacing as cartographer.launch.py: everything under /dogzilla, with
# tf remapped to the relative names so it lands on /dogzilla/tf{,_static}.
NAMESPACE = 'dogzilla'
TF_REMAPS = [('/tf', 'tf'), ('/tf_static', 'tf_static')]

# icp_odometry parameters. RTAB-Map takes all of its algorithm parameters as
# strings. Values follow RTAB-Map's own 2D-lidar ICP examples.
ICP_PARAMS = {
    'frame_id': 'base_link',
    'odom_frame_id': 'odom',
    'publish_tf': True,
    'wait_for_transform': 0.2,
    'Reg/Strategy': '1',          # ICP
    'Reg/Force3DoF': 'true',      # x, y, yaw only: a 2D scan can't observe the rest
    'Icp/PointToPlane': 'false',  # point-to-point; point-to-plane wants 3D normals
    'Icp/Iterations': '10',
    'Icp/VoxelSize': '0.05',
    'Icp/Epsilon': '0.001',
    'Icp/MaxTranslation': '2',
    'Icp/MaxCorrespondenceDistance': '1',
    'Icp/OutlierRatio': '0.7',
    'Icp/CorrespondenceRatio': '0.01',
    'Odom/ScanKeyFrameThr': '0.4',  # new keyframe when the overlap drops below this
}


def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time')

    # slam_toolbox's nodes are lifecycle nodes: configure, then activate.
    slam = LifecycleNode(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        namespace=NAMESPACE,
        output='screen',
        parameters=[CONFIG_FILE, {
            'use_sim_time': use_sim_time,
            'scan_topic': SCAN_TOPIC,
            # Default is the absolute /map; keep the map in our namespace.
            'map_name': '/' + NAMESPACE + '/map',
        }],
        remappings=TF_REMAPS,
    )

    configure = EmitEvent(event=ChangeState(
        lifecycle_node_matcher=matches_action(slam),
        transition_id=Transition.TRANSITION_CONFIGURE))

    activate = RegisterEventHandler(OnStateTransition(
        target_lifecycle_node=slam,
        start_state='configuring',
        goal_state='inactive',
        entities=[
            LogInfo(msg='slam_toolbox configured; activating'),
            EmitEvent(event=ChangeState(
                lifecycle_node_matcher=matches_action(slam),
                transition_id=Transition.TRANSITION_ACTIVATE)),
        ]))

    # odometry:=identity -- odom->base_link fixed at identity (see the header).
    # Latched on tf_static.
    identity_odom = Node(
        condition=LaunchConfigurationEquals('odometry', 'identity'),
        package='tf2_ros',
        executable='static_transform_publisher',
        name='identity_odom',
        namespace=NAMESPACE,
        arguments=['--frame-id', 'odom', '--child-frame-id', 'base_link'],
        remappings=TF_REMAPS,
    )

    # odometry:=icp -- scan-matching odometry; publishes odom->base_link on /tf.
    icp_odometry = Node(
        condition=LaunchConfigurationEquals('odometry', 'icp'),
        package='rtabmap_odom',
        executable='icp_odometry',
        name='icp_odometry',
        namespace=NAMESPACE,
        output='screen',
        parameters=[ICP_PARAMS, {'use_sim_time': use_sim_time}],
        remappings=[('scan', SCAN_TOPIC)] + TF_REMAPS,
    )

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('odometry', default_value='identity',
                              choices=['identity', 'icp'],
                              description='Source of odom->base_link'),
        identity_odom,
        icp_odometry,
        slam,
        configure,
        activate,
    ])
