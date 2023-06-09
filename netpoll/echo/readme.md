## intro
echo server, just check epoll io_uring in linux; know more linux applications performance history: <br>
https://unixism.net/2019/04/linux-applications-performance-introduction/

## echo-server
from: 
1. https://github.com/frevib/epoll-echo-server
2. https://github.com/frevib/io_uring-echo-server


## make && run server
```shell
make build-echo


# run single server to bench
./build/epoll_echo_server 8883
./build/io_uring_echo_server 8884
./build/coroutine_io_uring_echo_server 8882
cargo run -q --manifest-path netpoll/echo/rust-iouring-server/Cargo.toml --release -- 8881
```

## benchmarks
* Echo server is assigned a dedicated CPU with `taskset -cp 0 [pid]`
```shell
# set cpu bind server porcessor
sh ./netpoll/echo/taskset.sh 8883
sh ./netpoll/echo/taskset.sh 8884
sh ./netpoll/echo/taskset.sh 8882
sh ./netpoll/echo/taskset.sh 8881

# run bench
chmod +x ./netpoll/echo/bench.sh
./netpoll/echo/bench.sh 8883 epoll_echo_server
./netpoll/echo/bench.sh 8884 io_uring_echo_server
./netpoll/echo/bench.sh 8882 coroutine_io_uring_echo_server
./netpoll/echo/bench.sh 8881 rust_io_uring_echo_server

# or just bench all for avg result
make bench-echo
```

* Rust echo bench: https://github.com/haraldh/rust_echo_bench 
```shell
ulimit -n 10240
# eg:
cargo run --release -- --address "127.0.0.1:8883" --number 1000 --duration 60 --length 512
cargo run --release -- --address "127.0.0.1:8884" --number 1000 --duration 60 --length 512
```

## benchmark results QPS
