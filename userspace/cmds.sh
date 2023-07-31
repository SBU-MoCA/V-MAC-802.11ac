#!/bin/sh

service network-manager stop
ip link set wlx00c0caa8f2a8 down
iwconfig wlx00c0caa8f2a8 mode monitor
ip link set wlx00c0caa8f2a8 up
iw dev wlx00c0caa8f2a8 set channel 44 HT40+ 
