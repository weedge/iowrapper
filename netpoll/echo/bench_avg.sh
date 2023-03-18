#!/bin/bash
set -e
echo $(uname -a)

if [ "$#" -ne 2 ]; then
    echo "Please give port where echo server is running: $0 [echo_bin] [port]"
    exit
fi

curDate=`date +"%Y-%m-%d-%H:%M:%S"`
curDir=$(cd `dirname $0`; pwd)
#cd $curDir/rust_echo_bench

#connectionsArr=(1 50 150 300 500 1000)
connectionsArr=(2000)

for bytes in 128 512 1000; do
  for connections in ${connectionsArr[*]}; do
    echo "run benchmarks with c = $connections and len = $bytes"
    RPS_SUM=0
    for i in `seq 1 5`; do
      $1 $2 &
      sleep 1s
      SRV_PID=$(lsof -itcp:$2 | sed -n -e 2p | awk '{print $2}')
      taskset -cp 0 $SRV_PID

      OUT=`cargo run -q --manifest-path $curDir/rust_echo_bench/Cargo.toml --release -- --address "127.0.0.1:$2" --number $connections --duration 30 --length $bytes`
      RPS=$(echo "${OUT}" | sed -n '/^Speed/ p' | sed -r 's|^([^.]+).*$|\1|; s|^[^0-9]*([0-9]+).*$|\1 |')
      RPS_SUM=$((RPS_SUM + RPS))
      echo "attempt: $i, rps: $RPS "

      kill $SRV_PID
      sleep 1s
    done

    RPS_AVG=$((RPS_SUM/5))
    echo "average RPS: $RPS_AVG "
  done
done