#!/bin/sh
date +"%Y-%m-%d %H:%M:%S %w" | $(dirname $0)/../../gcode-cli/gcode-cli -s100 - /dev/ttyACM0
