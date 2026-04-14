import os
from launch import LaunchDescription
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch.substitutions import Command

URDF_PATH = os.path.join(os.path.dirname(__file__), 'dogzilla.urdf')

def generate_launch_description():
    robot_description = ParameterValue(
        Command(['cat ', URDF_PATH]),
        value_type=str,
    )

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description}],
    )

    return LaunchDescription([
        robot_state_publisher,
        # Add hardware driver nodes here (motor controllers, sensors, etc.)
    ])
