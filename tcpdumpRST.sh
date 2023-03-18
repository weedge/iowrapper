#!/bin/bash
set -e

#sudo tcpdump -n -v 'tcp[tcpflags] & (tcp-rst) != 0'
sudo tcpdump -ilo -n -v 'tcp[tcpflags] & (tcp-rst) != 0'
