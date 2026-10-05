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
  use_pose_extrapolator = false,
  use_odometry = false,
  use_nav_sat = false,
  use_landmarks = false,
  num_laser_scans = 1,
  num_multi_echo_laser_scans = 0,
  num_subdivisions_per_laser_scan = 1,
  num_point_clouds = 0,
  lookup_transform_timeout_sec = 0.2,
  submap_publish_period_sec = 0.3,
  pose_publish_period_sec = 0.05,
  trajectory_publish_period_sec = 0.05,
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
-- Widen how far (in meters) Cartographer will search to match a scan
TRAJECTORY_BUILDER_2D.real_time_correlative_scan_matcher.linear_search_window = 0.15
-- Widen how far (in radians) it searches during fast robot rotations
TRAJECTORY_BUILDER_2D.real_time_correlative_scan_matcher.angular_search_window = math.rad(30.0) -- Default is ~20°
TRAJECTORY_BUILDER_2D.motion_filter.max_angle_radians = math.rad(0.2)


-- Prevent a bad scan from breaking the match weights:
-- Lower the weight of the "Ceres" local optimizer so it trusts
-- online correlative search more than its previous velocity estimate.
TRAJECTORY_BUILDER_2D.ceres_scan_matcher.occupied_space_weight = 10.0
TRAJECTORY_BUILDER_2D.ceres_scan_matcher.translation_weight = 1.0
TRAJECTORY_BUILDER_2D.ceres_scan_matcher.rotation_weight = 1.0

-- Deal with the gait rocking:
-- If the LiDAR tilts, it sees fewer points or points at wrong distances.
-- Lowering the minimum score keeps Cartographer from instantly giving up.
TRAJECTORY_BUILDER_2D.min_range = 0.15
TRAJECTORY_BUILDER_2D.max_range = 12.0
TRAJECTORY_BUILDER_2D.motion_filter.max_angle_radians = math.rad(2.0)

POSE_GRAPH.constraint_builder.min_score = 0.50
POSE_GRAPH.constraint_builder.global_localization_min_score = 0.55

-- Tell the global optimizer to run more frequently to fix mistakes
POSE_GRAPH.optimize_every_n_nodes = 20  -- Lower numbers mean faster corrections

return options
