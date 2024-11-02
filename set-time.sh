#!/bin/sh

# Using gcode-cli, because it is very good in sending a line of text to
# a tty and waiting for the OK result https://github.com/hzeller/gcode-cli
date +"%Y-%m-%d %H:%M:%S %w" | gcode-cli -s100 - /dev/ttyACM0
