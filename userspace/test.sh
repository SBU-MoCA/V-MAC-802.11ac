#!/bin/sh

sudo ifconfig wlx00c0caa8f2a2 down
iwconfig wlx00c0caa8f2a2 channel 44 essid test mode ad-hoc
ifconfig wlx00c0caa8f2a2 up

