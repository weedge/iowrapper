### echo-server
from: 
1. https://github.com/frevib/epoll-echo-server
2. https://github.com/frevib/io_uring-echo-server

### benchmark
* Rust echo bench: https://github.com/haraldh/rust_echo_bench 
```shell
ulimit -n 10240
cargo run --release -- --address "127.0.0.1:8883" --number 1000 --duration 60 --length 512
cargo run --release -- --address "127.0.0.1:8884" --number 1000 --duration 60 --length 512
```
