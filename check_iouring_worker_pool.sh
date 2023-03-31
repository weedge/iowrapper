#!/bin/bash

set -e

udp_read=netpoll/udp/iouring-worker-pool/target/release/udp-read

make build-udp

sudo apt install sysstat
sudo apt install numactl
sudo apt-get install linux-tools-common linux-tools-generic linux-tools-`uname -r`
sudo apt install bpftrace

strace -e io_uring_register $udp_read --async --workers 8 &
pstree -pt $!
sleep 3 && kill $!


echo "NUMA cpu affinity"
numactl -H
echo "\n$udp_read --async --threads 1 --rings 1 --workers 2"
$udp_read --async --threads 1 --rings 1 --workers 2 &
pstree -pt $!
sleep 1 && kill $!

echo "\nstrace -e sched_setaffinity,io_uring_enter $udp_read --async --threads 1 --rings 2 --cpu 0 --cpu 2 --workers 2 & sleep 0.1 && echo"
strace -e sched_setaffinity,io_uring_enter $udp_read --async --threads 1 --rings 2 --cpu 0 --cpu 2 --workers 2 & sleep 0.1 && echo
pstree -pt $!
sleep 1 && kill $!

echo "\nasync 1 threads 2 rings 2 workers"
unshare -U $udp_read --async --threads 1 --rings 2 --workers 2 &
pstree -pt $!
ls -l /proc/$!/fd
sleep 1 && kill $!

echo "\nasync 2 threads 1 rings 2 workers"
unshare -U $udp_read --async --threads 2 --rings 1 --workers 2 &
pstree -pt $!
ls -l /proc/$!/fd
sleep 1 && kill $!



echo "\nprlimit --nproc=4 async 2 threads 1"
unshare -U prlimit --nproc=4 $udp_read --async --threads 2 --rings 1 &
pid=$!
pstree -pt $pid
ls -l /proc/$!/fd
#perf stat
sudo perf list 'io_uring:*'
sudo perf stat -e io_uring:* -p $pid --timeout 3000
sudo perf stat -a -d -d -d -p $pid --timeout 3000
#check io thread, io_queue_sqe() -> io_queue_async_work() -> create_io_worker() → create_io_thread()
sudo bpftrace -l | grep -e create_io_worker -e create_io_thread
#https://elixir.bootlin.com/linux/v6.1/source/kernel/fork.c#L2606 create kenerl io thread
sudo bpftrace --btf -e 'kretprobe:create_io_thread { @[retval] = count(); } interval:s:1 { print(@); clear(@); } END { clear(@); }' -c '/usr/bin/sleep 3' | cat -s
mpstat -P ALL 1 5
sleep 1 && kill $pid


