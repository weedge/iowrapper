liburing_dir = ../../../iouring/liburing
build_dir = ../../../build

CCFLAGS ?= -Wall -O3 -D_GNU_SOURCE
DEBUG_CCFLAGS ?= -g -Wall -O0 -D_GNU_SOURCE
LDFLAGS ?= -luring 
LIBURING_FLAGS ?= -I $(liburing_dir)/src/include

ifeq ($(DEBUG),1)
CCFLAGS = $(DEBUG_CCFLAGS)
endif

srcs := \
	io_uring_echo_server.c \
	io_uring_echo_server_v1.c \
	io_uring_echo_server_v2.c \
	io_uring_echo_server_v3.c \

all_targets := $(patsubst %.c,%,$(srcs))

all: $(all_targets)

pre:
	@mkdir -p ${build_dir}

clean: pre
	@cd ${build_dir} && rm $(all_targets)

liburing:
	+$(MAKE) -C ${liburing_dir}

%: %.c pre
	$(CC) $(CCFLAGS) ${LIBURING_FLAGS} -o ${build_dir}/$@ $< ${liburing_dir}/src/liburing.a

.PHONY: liburing


