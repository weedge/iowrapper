
#!/bin/bash
set -e

if [ "$#" -lt 1 ]; then
    echo "Please give port: $0 [port] [mod]"
    exit
fi

pid=$(lsof -itcp:$1 | sed -n -e 2p | awk '{print $2}')
echo "pid $pid"

if [ $2 = "strace" ]; then
    sudo strace -p $pid
    exit
fi

if [ $2 = "perf" ]; then
    sudo perf trace -p $pid
    exit
fi

#sudo bpftrace -l | grep io_uring
if [ $2 = "bpftrace" ]; then
    if [ -z $3 ]; then
        sudo bpftrace -e 'tracepoint:io_uring:io_uring_submit_sqe {printf("%s(%d)\n", comm, pid);}'
    elif [ $3 = "thread" ]; then
        sudo bpftrace --btf -e 'kretprobe:create_io_thread { @[retval] = count(); } interval:s:1 { print(@); clear(@); } END { clear(@); }' | cat -s
    else
	# tracepoint:syscalls:sys_enter_io_uring_enter | tracepoint:syscalls:sys_exit_io_uring_register
        sudo bpftrace -e 'tracepoint:'$3' {printf("%s(%d)\n", comm, pid);}'
    fi
    exit
fi

watch -n 1 "lsof -itcp:$1 | sed -n -e 2p | awk '{print \$2}' | xargs pstree -pt"


#sh watchPort.sh 8888
#sh watchPort.sh 8888 strace
#sh watchPort.sh 8888 bpftrace
#sh watchPort.sh 8888 bpftrace thread
#sh watchPort.sh 8888 bpftrace syscalls:sys_enter_io_uring_enter