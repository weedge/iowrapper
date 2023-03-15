CCFLAGS ?= -Wall -O3 -D_GNU_SOURCE -luring
all_targets = io_uring_echo_server
build_dir = ../../../build

.PHONY: liburing io_uring_echo_server

all: $(all_targets)

clean:
	rm -f ${build_dir}/$(all_targets)

build: clean
	@mkdir -p ${build_dir}

liburing:
	+$(MAKE) -C ../../../iouring/liburing

io_uring_echo_server:
	$(CC) main.c -o ${build_dir}/io_uring_echo_server  ${CCFLAGS}

debug:
	$(CC) main.c -o ${build_dir}/io_uring_echo_server  ${CCFLAGS} -g