SHELL := /bin/bash
check_iouring_worker_pool?=./check_iouring_worker_pool.sh
echo_bench_result_dir?=./netpoll/echo/bench-result
echo_bench_avg_shell?=./netpoll/echo/bench_avg.sh

#boost_fiber_uring_proactor_build_dir?=boost_fiber_uring_proactor/build-opt
boost_fiber_uring_proactor_build_dir?=${HOME}/project/c++/helio/build-opt

target ?= \
	golang_netpoll_more_echo_server \
	golang_reactor_echo_server \
	golang_multi_iouring_echo_server \
	golang_multi_iouring_sqp_echo_server \
	#boost_fiber_uring_echo_server \
	#golang_netpoll_echo_server \
	#golang_iouring_echo_server \
	#golang_iouring_sqp_echo_server \
	#c_epoll_echo_server \
	#c_io_uring_echo_server \
	#c_io_uring_echo_server_v3 \
	#c_io_uring_echo_server_sqp \
	#cpp20_coroutine_io_uring_echo_server \
	#rust_io_uring_echo_server \

help:
	@echo "build-echo"
	@echo "bench-echo"

pre:
	@mkdir -p ${echo_bench_result_dir}
	@chmod +x ${echo_bench_avg_shell}

cargo:
	@curl https://sh.rustup.rs -sSf | sh
	@source "${HOME}/.cargo/env"

init:
	@git submodule init
	@git submodule update
	@make -C iouring/liburing

build-echo: init
	@make -C netpoll/echo/c-epoll-server
	@make -C netpoll/echo/c-iouring-server
	@make -C netpoll/echo/cpp-coroutine-iouring-server
	@cargo build --manifest-path netpoll/echo/rust-iouring-server/Cargo.toml --release
	@CGO_ENABLED=0 go build -v -ldflags="-s -w" -o ./build/golang_netpoll_echo_server netpoll/echo/golang-netpoll-server/main.go
	@CGO_ENABLED=0 go build -v -ldflags="-s -w" -tags goiouring -o ./build/golang_iouring_echo_server netpoll/echo/golang-iouring-server/main.go
	@CGO_ENABLED=0 go build -v -ldflags="-s -w" -o ./build/golang_reactor_echo_server netpoll/echo/golang-reactor-epoll-server/main.go
	@CGO_ENABLED=0 go build -v -ldflags="-s -w" -o ./build/golang_multi_iouring_echo_server netpoll/echo/golang-multi-iouring-server/main.go

build-udp:
	@cargo build --manifest-path netpoll/udp/iouring-worker-pool/Cargo.toml --release

bench-echo: pre ${target}

golang_iouring_echo_server:
	#bench golang_iouring_echo_server
	@${echo_bench_avg_shell} 8888 "./build/golang_iouring_echo_server 8888" c1 \
		>> ${echo_bench_result_dir}/golang_iouring_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

golang_iouring_sqp_echo_server:
	#bench golang_iouring_sqp_echo_server
	@${echo_bench_avg_shell} 8888 "./build/golang_iouring_echo_server 8888 sqp" c1 \
		>> ${echo_bench_result_dir}/golang_iouring_sqp_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

golang_netpoll_echo_server:
	#bench golang_netpoll_echo_server
	@${echo_bench_avg_shell} 8888 "./build/golang_netpoll_echo_server 8888" c1 \
		>> ${echo_bench_result_dir}/golang_netpoll_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

c_epoll_echo_server:
	#bench c_epoll_echo_server
	@${echo_bench_avg_shell} 8883 "./build/epoll_echo_server 8883" c1 \
		>> ${echo_bench_result_dir}/epoll_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

c_io_uring_echo_server:
	#bench io_uring_echo_server
	@${echo_bench_avg_shell} 8884 "./build/io_uring_echo_server 8884" c1 \
		>> ${echo_bench_result_dir}/io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1
c_io_uring_echo_server_v1:
	#bench io_uring_echo_server_v1
	@${echo_bench_avg_shell} 8884 "./build/io_uring_echo_server_v1 8884" c1 \
		>> ${echo_bench_result_dir}/io_uring_echo_server_v1.`date +"%Y%m%d-%H%M%S"`.log 2>&1
c_io_uring_echo_server_v2:
	#bench io_uring_echo_server_v2
	@${echo_bench_avg_shell} 8884 "./build/io_uring_echo_server_v2 8884" c1 \
		>> ${echo_bench_result_dir}/io_uring_echo_server_v2.`date +"%Y%m%d-%H%M%S"`.log 2>&1
c_io_uring_echo_server_v3:
	#bench io_uring_echo_server_v3
	@${echo_bench_avg_shell} 8884 "./build/io_uring_echo_server_v3 8884" c1 \
		>> ${echo_bench_result_dir}/io_uring_echo_server_v3.`date +"%Y%m%d-%H%M%S"`.log 2>&1

c_io_uring_echo_server_sqp:
	#bench io_uring_echo_server_sqp
	@${echo_bench_avg_shell} 8884 "./build/io_uring_echo_server 8884 sqp" c1 \
		>> ${echo_bench_result_dir}/io_uring_echo_server_sqp.`date +"%Y%m%d-%H%M%S"`.log 2>&1

cpp20_coroutine_io_uring_echo_server:
	#bench coroutine_io_uring_echo_server
	@${echo_bench_avg_shell} 8882 "./build/coroutine_io_uring_echo_server 8882" c1 \
		>> ${echo_bench_result_dir}/coroutine_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

rust_io_uring_echo_server:
	#bench rust_io_uring_echo_server
	@${echo_bench_avg_shell} 8881 "netpoll/echo/rust-iouring-server/target/release/rust-iouring-server 8881" c1 \
		>> ${echo_bench_result_dir}/rust_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

check_iouring_worker_pool:
	@${check_iouring_worker_pool}


#build-boost_fiber_uring_echo_server:
#	@cd boost_fiber_uring_proactor
#	@sudo ./install-dependencies.sh
#	@bash ./blaze.sh -ninja -release
#	@cd build-opt && ninja -j4 raw_echo_server

# make bench-echo target=1c1t_boost_fiber_uring_echo_server
1c1t_boost_fiber_uring_echo_server:
	#bench 1c1tfiber_io_uring_echo_server
	@${echo_bench_avg_shell} 8888 "${boost_fiber_uring_proactor_build_dir}/raw_echo_server --logtostderr --threads=1 --port=8888" size c1 \
		>> ${echo_bench_result_dir}/1c1t_fiber_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

# make bench-echo target=boost_fiber_uring_echo_server
boost_fiber_uring_echo_server:
	#bench fiber_io_uring_echo_server
	@${echo_bench_avg_shell} 8888 "${boost_fiber_uring_proactor_build_dir}/raw_echo_server --logtostderr --threads=4 --port=8888" size \
		>> ${echo_bench_result_dir}/fiber_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

# make bench-echo target=golang_reactor_echo_server
golang_reactor_echo_server:
	#bench golang_reactor_echo_server
	@${echo_bench_avg_shell} 8888 "./build/golang_reactor_echo_server --port 8888" size \
		>> ${echo_bench_result_dir}/golang_reactor_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

# make bench-echo target=golang_netpoll_more_echo_server
golang_netpoll_more_echo_server:
	#bench golang_netpoll_more_echo_server
	@${echo_bench_avg_shell} 8888 "./build/golang_netpoll_echo_server 8888" \
		>> ${echo_bench_result_dir}/golang_netpoll_more_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

# make bench-echo target=golang_multi_iouring_echo_server
golang_multi_iouring_echo_server:
	#bench golang_multi_iouring_echo_server
	@${echo_bench_avg_shell} 8888 "./build/golang_multi_iouring_echo_server -port 8888" \
		>> ${echo_bench_result_dir}/golang_multi_iouring_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1

# make bench-echo target=golang_multi_iouring_sqp_echo_server
golang_multi_iouring_sqp_echo_server:
	#bench golang_multi_iouring_sqp_echo_server
	@${echo_bench_avg_shell} 8888 "./build/golang_multi_iouring_echo_server -port 8888 -mode sqp" \
		>> ${echo_bench_result_dir}/golang_multi_iouring_sqp_echo_server.`date +"%Y%m%d-%H%M%S"`.log 2>&1