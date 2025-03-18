#!/bin/bash
sudo \
SERVER_NAME="Test" \
INTERFACE="eth0" \
MOUNT="/" \
URL="http://localhost:5000/report" \
TIME="5" \
dpkg -i ./agent.deb
sudo cat /etc/monitoring_agent.conf