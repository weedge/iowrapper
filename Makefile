echo_bench_result_dir?=./netpoll/echo/bench-result

build-echo:
	@make -C netpoll/echo/c-epoll-server
	@make -C netpoll/echo/c-iouring-server
	@make -C netpoll/echo/cpp-coroutine-iouring-server

mkdir-echo-bench-result:
	@mkdir -p ${echo_bench_result_dir}

bench-echo: mkdir-echo-bench-result
	@sh ./netpoll/echo/bench_avg.sh ./build/epoll_echo_server 8883 \
		>> ${echo_bench_result_dir}/epoll_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1
	@sh ./netpoll/echo/bench_avg.sh ./build/io_uring_echo_server 8884 \
		>> ${echo_bench_result_dir}/io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1
	@sh ./netpoll/echo/bench_avg.sh ./build/coroutine_io_uring_echo_server 8882 \
		>> ${echo_bench_result_dir}/coroutine_io_uring_echo_server.`date +"%Y%m%d-%H%M%S"`.txt 2>&1