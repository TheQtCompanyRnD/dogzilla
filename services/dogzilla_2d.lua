-- Copyright (C) 2026 The Qt Company Ltd.
-- SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
--
-- Cartographer 2D configuration for the Dogzilla S2 + Oradar MS200 lidar.
-- map_builder.lua / trajectory_builder.lua resolve from the installed
-- cartographer_ros configuration_files directory.

include "map_builder.lua"
include "trajectory_builder.lua"

options = {
  map_builder = MAP_BUILDER,
  trajectory_builder = TRAJECTORY_BUILDER,
  map_frame = "map",
  -- No IMU, so we track in base_link; scans arrive in laser_frame and are
  -- transformed via the static base_link->laser_frame TF from dogzillad.
  tracking_frame = "base_link",
  published_frame = "base_link",
  odom_frame = "odom",
  -- We have no external odometry: let Cartographer create and publish the
  -- full map->odom->base_link chain from scan matching alone.
  provide_odom_frame = true,
  publish_frame_projected_to_2d = true,
  use_pose_extrapolator = true,
  use_odometry = false,
  use_nav_sat = false,
  use_landmarks = false,
  num_laser_scans = 1,
  num_multi_echo_laser_scans = 0,
  num_subdivisions_per_laser_scan = 1,
  num_point_clouds = 0,
  lookup_transform_timeout_sec = 0.2,
  submap_publish_period_sec = 0.3,
  pose_publish_period_sec = 5e-3,
  trajectory_publish_period_sec = 30e-3,
  rangefinder_sampling_ratio = 1.,
  odometry_sampling_ratio = 1.,
  fixed_frame_pose_sampling_ratio = 1.,
  imu_sampling_ratio = 1.,
  landmarks_sampling_ratio = 1.,
}

MAP_BUILDER.use_trajectory_builder_2d = true

-- MS200: ~0.05-12 m usable range.
TRAJECTORY_BUILDER_2D.use_imu_data = false
TRAJECTORY_BUILDER_2D.min_range = 0.05
TRAJECTORY_BUILDER_2D.max_range = 12.0
TRAJECTORY_BUILDER_2D.missing_data_ray_length = 3.0
-- With no odometry prior, the correlative matcher makes the front-end far more
-- robust to the motion between scans (and to the gait rocking the lidar).
TRAJECTORY_BUILDER_2D.use_online_correlative_scan_matching = true
TRAJECTORY_BUILDER_2D.motion_filter.max_angle_radians = math.rad(0.2)

POSE_GRAPH.constraint_builder.min_score = 0.65
POSE_GRAPH.constraint_builder.global_localization_min_score = 0.7

return options
