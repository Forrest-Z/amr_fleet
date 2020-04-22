# amr_fleet
AMR (Automous Mobile Robots) fleet management system


## Install packages dependencies
__Yaml-cpp__:
```sudo apt install libyaml-cpp-dev```

__Dxflib__:
```sudo apt install libdxflib-dev ```

How to install __multimaster_fkie__: https://github.com/fkie/multimaster_fkie

To install all __ROS dependencies__, run command from workspace root:
```rosdep install --from-paths src --ignore-src --rosdistro melodic -y```

## Mapping
Run:
```
roslaunch amr_fleet mapping.launch 
```

Use package:
https://github.com/ros-teleop/teleop_twist_keyboard

and run:
```
rosrun teleop_twist_keyboard teleop_twist_keyboard.py cmd_vel:="/r1/mobile_base/commands/velocity"
```

For save the map:
```
rosrun map_server map_saver -f map
```
