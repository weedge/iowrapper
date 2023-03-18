#!/bin/bash
# https://mp.weixin.qq.com/s/Fr6o6gRiIUIspV9-jR9snw

set -e

#sudo tcpdump -n -v 'tcp[tcpflags] & (tcp-rst) != 0'
sudo tcpdump -ilo -n -v 'tcp[tcpflags] & (tcp-rst) != 0'

