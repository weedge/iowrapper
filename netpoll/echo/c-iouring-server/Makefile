liburing_dir = ../../../iouring/liburing
all_targets = io_uring_echo_server-local
build_dir = ../../../build

CCFLAGS ?= -Wall -O3 -D_GNU_SOURCE 
DEBUG_CCFLAGS ?= -Wall -O0 -D_GNU_SOURCE -luring -g
LIBURING_FLAGS ?= -I $(liburing_dir)/include

.PHONY: liburing io_uring_echo_server

all: $(all_targets)

clean:
	rm -f ${build_dir}/$(all_targets)

build: clean
	@mkdir -p ${build_dir}

liburing:
	+$(MAKE) -C ${liburing_dir}

io_uring_echo_server:
	$(CC) main.c -o ${build_dir}/io_uring_echo_server -luring ${CCFLAGS} 

debug:
	$(CC) main.c -o ${build_dir}/io_uring_echo_server -luring ${DEBUG_CCFLAGS}

io_uring_echo_server-local:
	$(CC) main.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server  ${CCFLAGS} ${LIBURING_FLAGS}

debug-local:
	$(CC) main.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server  ${DEBUG_CCFLAGS} ${LIBURING_FLAGS}

io_uring_echo_server_v1-local:
	$(CC) io_uring_echo_server_v1.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server_v1  ${CCFLAGS} ${LIBURING_FLAGS}

debug_v1-local:
	$(CC) io_uring_echo_server_v1.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server_v1  ${DEBUG_CCFLAGS} ${LIBURING_FLAGS}

io_uring_echo_server_v2-local:
	$(CC) io_uring_echo_server_v2.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server_v2  ${CCFLAGS} ${LIBURING_FLAGS}

debug_v2-local:
	$(CC) io_uring_echo_server_v2.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server_v2  ${DEBUG_CCFLAGS} ${LIBURING_FLAGS}

io_uring_echo_server_v3-local:
	$(CC) io_uring_echo_server_v3.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server_v3  ${CCFLAGS} ${LIBURING_FLAGS}

debug_v3-local:
	$(CC) io_uring_echo_server_v3.c ${liburing_dir}/src/liburing.a -o ${build_dir}/io_uring_echo_server_v3  ${DEBUG_CCFLAGS} ${LIBURING_FLAGS}