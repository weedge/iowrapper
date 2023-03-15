
build-echo:
	@make -C netpoll/echo/c-epoll-server
	@make -C netpoll/echo/c-iouring-server
	@make -C netpoll/echo/cpp-coroutine-iouring-server
