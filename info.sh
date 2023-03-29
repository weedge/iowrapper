#!/bin/bash
set -e

lsb_release -a

uname -a

echo "physical id"
cat /proc/cpuinfo |grep "physical id"|sort |uniq|wc -l

echo "processor"
cat /proc/cpuinfo |grep "processor"|wc -l

cat /proc/cpuinfo |grep MHz|uniq

cat /proc/cpuinfo | grep name | cut -f2 -d: | uniq -c

grep MemTotal /proc/meminfo