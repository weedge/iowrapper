liburing_dir = ../../../iouring/liburing
build_dir = ../../../build

CXXFLAGS ?= -Wall -O3 -D_GNU_SOURCE -std=c++20 -fcoroutines
DEBUG_CXXFLAGS ?= -g -Wall -O0 -D_GNU_SOURCE -std=c++20 -fcoroutines
LDFLAGS ?= luring
LIBURING_FLAGS ?= -I $(liburing_dir)/src/include

ifeq ($(DEBUG),1)
CXXFLAGS = $(DEBUG_CXXFLAGS)
endif

srcs := \
  	coroutine_io_uring_echo_server.cpp \
	
all_targets := $(patsubst %.cpp,%,$(srcs))


all: $(all_targets)

clean:
	rm -f $(build_dir)/${all_targets}

build: clean
	@mkdir -p ${build_dir}

liburing:
	+$(MAKE) -C ../../../iouring/liburing

%: %.cpp clean liburing
	$(CXX) $(CXXFLAGS) ${LIBURING_FLAGS} -o ${build_dir}/$@ $< ${liburing_dir}/src/liburing.a

