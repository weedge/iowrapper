#!/bin/bash
set -e
echo $(uname -a)

if [ "$#" -ne 2 ]; then
    echo "Please give port where echo server is running: $0 [port] [bench result file name]"
    exit
fi


curDate=`date +"%Y-%m-%d-%H:%M:%S"`
curDir=$(cd `dirname $0`; pwd)
cd $curDir/rust_echo_bench

connectionsArr=(1 50 150 300 500 1000 2000)
#connectionsArr=(3000)
ulimit -n 10240

for bytes in 128 512 1000; do
	for connections in ${connectionsArr[*]}; do
    echo "cargo run --release -- --address 'localhost:$1' --number $connections --duration 60 --length $bytes \
        >> $curDir/bench-result/$2.$curDate.log 2>&1"
   	cargo run --release -- --address "localhost:$1" --number $connections --duration 60 --length $bytes \
        >> $curDir/bench-result/$2.$curDate.log 2>&1
   	sleep 3
	done
done
