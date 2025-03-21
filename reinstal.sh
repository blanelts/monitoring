#!/bin/bash
sudo \
SERVER_NAME="$(hostname)" \
INTERFACE="eth0" \
MOUNT="/" \
URL="http://192.168.160.37:5050/report" \
TIME="5" \
dpkg -i ./agent.deb
sudo cat /etc/monitoring_agent.conf