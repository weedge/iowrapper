echo_bench_result_dir?=./netpoll/echo/bench-result
echo_bench_avg_shell?=./netpoll/echo/bench_avg.sh

build-echo:
	@make -C netpoll/echo/c-epoll-server
	@make -C netpoll/echo/c-iouring-server
	@make -C netpoll/echo/cpp-coroutine-iouring-server

mkdir-echo-bench-result:
	@mkdir -p ${echo_bench_result_dir}

bench-echo: mkdir-echo-bench-result
	@chmod +x ${echo_bench_avg_shell}
	@${echo_bench_avg_shell} ./build/epoll_echo_server 8883 \
		>> ${echo_bench_result_dir}/epoll_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1
	@${echo_bench_avg_shell} ./build/io_uring_echo_server 8884 \
		>> ${echo_bench_result_dir}/io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1
	@${echo_bench_avg_shell} ./build/coroutine_io_uring_echo_server 8882 \
		>> ${echo_bench_result_dir}/coroutine_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1