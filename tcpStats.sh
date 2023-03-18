#!/bin/bash
set -e

if [ "$#" -ne 1 ]; then
    echo "Please give port: $0 [port]"
    exit
fi

while true; do
    printf "server port $1 ESTABLISHED: "
    netstat -ta | grep $1 | grep ESTABLISHED -w | wc -l
    printf "server port $1 CLOSE_WAIT: "
    netstat -ta | grep $1 | grep CLOSE_WAIT -w | wc -l
    printf "connect port $1 TIME_WAIT: "
    netstat -ta | grep $1 | grep TIME_WAIT -w | wc -l
    printf "\n"
    sleep 1
done
