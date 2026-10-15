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
# To feed it real odometry later (e.g. rtabmap_odom icp_odometry), drop the
# static transform below and let that node publish odom->base_link.
#
# Usage:  ros2 launch slam_toolbox.launch.py

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, EmitEvent, LogInfo, RegisterEventHandler
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

    # Identity odom->base_link (see the header). Latched on tf_static.
    identity_odom = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='identity_odom',
        namespace=NAMESPACE,
        arguments=['--frame-id', 'odom', '--child-frame-id', 'base_link'],
        remappings=TF_REMAPS,
    )

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        identity_odom,
        slam,
        configure,
        activate,
    ])
