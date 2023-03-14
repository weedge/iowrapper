## echo-server
from: 
1. https://github.com/frevib/epoll-echo-server
2. https://github.com/frevib/io_uring-echo-server

## make && run server
```shell
cd netpoll/echo
make -C ./c-epoll-server
make -C ./c-iouring-server

./c-epoll-server/epoll_echo_server 8883
./c-iouring-server/io_uring_echo_server 8884
```

## benchmarks
* Echo server is assigned a dedicated CPU with `taskset -cp 0 [pid]`
```shell
#!/bin/bash
echo $(uname -a)

if [ "$#" -ne 1 ]; then
    echo "Please give port where echo server is running: $0 [port]"
    exit
fi

PID=$(lsof -itcp:$1 | sed -n -e 2p | awk '{print $2}')
taskset -cp 0 $PID

for bytes in 1 128 512 1000
do
	for connections in 1 50 150 300 500 1000
	do
   	cargo run --release -- --address "localhost:$1" --number $connections --duration 60 --length $bytes
   	sleep 4
	done
done
```

* Rust echo bench: https://github.com/haraldh/rust_echo_bench 
```shell
ulimit -n 10240
cargo run --release -- --address "127.0.0.1:8883" --number 1000 --duration 60 --length 512
cargo run --release -- --address "127.0.0.1:8884" --number 1000 --duration 60 --length 512
```

## benchmark results QPS
