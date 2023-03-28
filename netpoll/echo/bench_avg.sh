#!/bin/bash
set -e
echo $(uname -a)

if [ "$#" -lt 2 ]; then
    echo "Please give port where echo server is running: $0 [port] [serverCmd]"
    exit
fi
port=$1
serverCmd=$2

curDate=`date +"%Y-%m-%d-%H:%M:%S"`
curDir=$(cd `dirname $0`; pwd)
#cd $curDir/rust_echo_bench

connectionsArr=(300 500 1000 2000)
#connectionsArr=(2000)
bytesArr=(128 512 1000)
#bytesArr=(1000)

$serverCmd &
SRV_PID=$!
#SRV_PID=$(lsof -itcp:$2 | sed -n -e 2p | awk '{print $2}')
taskset -cp 0 $SRV_PID
sleep 3s

runCn=3
for bytes in ${bytesArr[*]}; do
  for connections in ${connectionsArr[*]}; do
    echo "run benchmarks with c = $connections and len = $bytes"
    RPS_SUM=0
    for i in `seq 1 $runCn`; do

      OUT=`cargo run -q --manifest-path $curDir/rust_echo_bench/Cargo.toml --release -- --address "127.0.0.1:$port" --number $connections --duration 30 --length $bytes`
      RPS=$(echo "${OUT}" | sed -n '/^Speed/ p' | sed -r 's|^([^.]+).*$|\1|; s|^[^0-9]*([0-9]+).*$|\1 |')
      RPS_SUM=$((RPS_SUM + RPS))
      echo "attempt: $i, rps: $RPS "

      sleep 3s
    done

    RPS_AVG=$((RPS_SUM/runCn))
    echo "average RPS: $RPS_AVG "
  done
done

kill $SRV_PID
sleep 3s