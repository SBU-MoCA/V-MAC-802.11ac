#!/bin/sh

airmon-ng check kill
ip link set wlan1 down
iw dev wlan1 set type monitor
ip link set wlan1 up
sudo iw dev wlan1 set channel 44 HT40+
wiw wlan1 set txpower fixed 3000
sudo service wpa_supplicant stop
#sudo service networking stop
sudo service network-manager stop
sudo service bluetooth stop
