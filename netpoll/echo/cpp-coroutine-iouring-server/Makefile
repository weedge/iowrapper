CXXFLAGS ?= -Wall -O3 -D_GNU_SOURCE -luring -std=c++20 -fcoroutines
all_targets = coroutine_io_uring_echo_server
build_dir = ../../../build

.PHONY: liburing io_uring_echo_server

all: $(all_targets)

clean:
	rm -f $(build_dir)/${all_targets}

build: clean
	@mkdir -p ${build_dir}

liburing:
	+$(MAKE) -C ../../../iouring/liburing

coroutine_io_uring_echo_server: clean
	$(CXX) main.cpp -o ${build_dir}/${all_targets}  ${CXXFLAGS}

debug: clean
	$(CXX) main.cpp -o ${build_dir}/${all_targets}  ${CXXFLAGS} -g