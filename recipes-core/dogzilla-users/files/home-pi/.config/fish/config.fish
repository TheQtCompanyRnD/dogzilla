set -g fish_prompt_pwd_dir_length 0
set -x QT_MESSAGE_PATTERN "[%{time process} %{if-debug}D%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}] %{category} - %{message}"

# ~/.config/fish/config.fish -- pre-populated from meta-dogzilla
# (meta-dogzilla/recipes-core/dogzilla-users/files/home-pi/). Edit here and
# rebuild to have it land in /home/pi; or rsync at runtime for quick iteration.
if status is-interactive
    set -g fish_greeting "dogzilla 🐕  ROS 2 Jazzy + Qt 6.12"
end

# Put the sbin dirs on PATH. A console login (HDMI + keyboard) gets
# PATH=/bin:/usr/bin from ENV_PATH in /etc/login.defs, which omits them, so
# `ip addr`, `wpa_cli` and the rest of /usr/sbin are simply not found there --
# while an ssh session has them, because sshd supplies its own PATH. Appended,
# not prepended, so /usr/bin still wins for any duplicated name. Root is
# unaffected either way: it gets ENV_SUPATH, which already lists sbin.
for dir in /usr/sbin /sbin
    if test -d $dir; and not contains $dir $PATH
        set -gx PATH $PATH $dir
    end
end

# ROS 2 Jazzy environment for interactive use (ros2 CLI, ros2 launch, etc).
# A static equivalent of sourcing /opt/ros/jazzy/setup.bash -- no `bass`/bash
# subprocess per shell, which is fine because this image's install prefix and
# Python version are fixed. Bump the python3.13 path if ROS/Python is updated.
# ROS_DOMAIN_ID is set separately in /etc/fish/conf.d/10-ros-domain.fish.
if test -d /opt/ros/jazzy
    set -gx ROS_DISTRO jazzy
    set -gx ROS_VERSION 2
    set -gx ROS_PYTHON_VERSION 3
    set -gx AMENT_PREFIX_PATH /opt/ros/jazzy
    set -gx --path LD_LIBRARY_PATH /opt/ros/jazzy/lib $LD_LIBRARY_PATH
    set -gx --path PYTHONPATH /opt/ros/jazzy/lib/python3.13/site-packages $PYTHONPATH
    set -gx --path CMAKE_PREFIX_PATH /opt/ros/jazzy $CMAKE_PREFIX_PATH
    if not contains /opt/ros/jazzy/bin $PATH
        set -gx PATH /opt/ros/jazzy/bin $PATH
    end
    if status is-interactive
        echo "ROS: $ROS_DISTRO"
    end
end
