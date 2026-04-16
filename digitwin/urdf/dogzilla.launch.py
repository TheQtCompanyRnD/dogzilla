import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration, Command, PythonExpression
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue

URDF_PATH = os.path.join(os.path.dirname(__file__), 'dogzilla.urdf')

def generate_launch_description():
    use_gui_arg = DeclareLaunchArgument(
        'use_gui',
        default_value='true',
        description='Launch joint_state_publisher_gui instead of joint_state_publisher',
    )

    use_gui = LaunchConfiguration('use_gui')

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

    # Headless joint state publisher (use_gui:=false)
    # NOTE: This isn't really useful for development, but
    # can be used in CI/Scripting or just restoring the orignal values.
    joint_state_publisher = Node(
        package='joint_state_publisher',
        executable='joint_state_publisher',
        name='joint_state_publisher',
        condition=UnlessCondition(use_gui),
    )

    # Interactive joint sliders (use_gui:=true, default)
    joint_state_publisher_gui = Node(
        package='joint_state_publisher_gui',
        executable='joint_state_publisher_gui',
        name='joint_state_publisher_gui',
        condition=IfCondition(use_gui),
    )

    return LaunchDescription([
        use_gui_arg,
        robot_state_publisher,
        joint_state_publisher,
        joint_state_publisher_gui,
    ])
