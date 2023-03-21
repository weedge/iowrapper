SHELL := /bin/bash
check_iouring_worker_pool?=./check_iouring_worker_pool.sh
echo_bench_result_dir?=./netpoll/echo/bench-result
echo_bench_avg_shell?=./netpoll/echo/bench_avg.sh
target ?= \
	golang_netpoll_echo_server \
	c_epoll_echo_server \
	#c_io_uring_echo_server \
	#cpp20_coroutine_io_uring_echo_server \
	#rust_io_uring_echo_server \


help:
	@echo "build-echo"
	@echo "bench-echo"
pre:
	@mkdir -p ${echo_bench_result_dir}
	@chmod +x ${echo_bench_avg_shell};

cargo:
	@curl https://sh.rustup.rs -sSf | sh
	@source "${HOME}/.cargo/env"

build-echo:
	@make -C netpoll/echo/c-epoll-server
	@make -C netpoll/echo/c-iouring-server
	@make -C netpoll/echo/cpp-coroutine-iouring-server
	@cargo build --manifest-path netpoll/echo/rust-iouring-server/Cargo.toml --release
	@go build -v -ldflags="-s -w" -o ./build/golang_netpoll_echo_server netpoll/echo/golang-netpoll-server/main.go

build-udp:
	@cargo build --manifest-path netpoll/udp/iouring-worker-pool/Cargo.toml --release

bench-echo: pre ${target}

golang_netpoll_echo_server:
	#bench golang_netpoll_echo_server
	@${echo_bench_avg_shell} ./build/golang_netpoll_echo_server 8880 \
		>> ${echo_bench_result_dir}/golang_netpoll_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1

c_epoll_echo_server:
	#bench c_epoll_echo_server
	@${echo_bench_avg_shell} ./build/epoll_echo_server 8883 \
		>> ${echo_bench_result_dir}/epoll_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1

c_io_uring_echo_server:
	#bench io_uring_echo_server
	@${echo_bench_avg_shell} ./build/io_uring_echo_server 8884 \
		>> ${echo_bench_result_dir}/io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1

cpp20_coroutine_io_uring_echo_server:
	#bench coroutine_io_uring_echo_server
	@${echo_bench_avg_shell} ./build/coroutine_io_uring_echo_server 8882 \
		>> ${echo_bench_result_dir}/coroutine_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1

rust_io_uring_echo_server:
	#bench rust_io_uring_echo_server
	@${echo_bench_avg_shell} netpoll/echo/rust-iouring-server/target/release/rust-iouring-server 8881 \
		>> ${echo_bench_result_dir}/rust_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1

check_iouring_worker_pool:
	@${check_iouring_worker_pool}

