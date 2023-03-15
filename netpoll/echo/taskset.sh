#!/bin/bash
echo $(uname -a)

if [ "$#" -ne 1 ]; then
    echo "Please give port where echo server is running: $0 [port]"
    exit
fi

PID=$(lsof -itcp:$1 | sed -n -e 2p | awk '{print $2}')
taskset -cp 0 $PID

