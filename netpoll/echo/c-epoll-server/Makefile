CCFLAGS ?= -Wall -O3 -D_GNU_SOURCE
all_targets = epoll_echo_server
build_dir = ../../../build

.PHONY: epoll_echo_server

all: $(all_targets)

clean:
	rm -f ${build_dir}/$(all_targets)

build: clean
	@mkdir -p ${build_dir}

epoll_echo_server: build
	$(CC) main.c -o ${build_dir}/epoll_echo_server ${CCFLAGS}

debug:
	$(CC) main.c -o ${build_dir}/epoll_echo_server ${CCFLAGS} -g