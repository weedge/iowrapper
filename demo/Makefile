liburing_dir = ../iouring/liburing
build_dir = ../build

CC ?= cc
CCFLAGS ?= -Wall -O3 -D_GNU_SOURCE 
DEBUG_CCFLAGS ?= -Wall -O0 -D_GNU_SOURCE -g
LDFLAGS ?= -luring 
LIBURING_FLAGS ?= -I $(liburing_dir)/include

# Please keep this list sorted alphabetically.
demo_srcs := \
	tcp_epoll_send_recvmsg_iouring_echo_demo.c \
	tcp_iouring_send_recvmsg_echo_demo.c \
	tcp_iouring_send_recv_provide_buff_echo_demo.c \
	tcp_iouring_send_recvmsg_provide_buff_echo_demo.c \

demo_targets := $(patsubst %.c,%,$(demo_srcs))
all_targets += $(demo_targets)

io_op = io_op.o

all: $(all_targets)

io_op.o: io_op.c ${liburing_dir}/src/liburing.a
	$(CC) ${DEBUG_CCFLAGS} ${LIBURING_FLAGS} -o ${build_dir}/$@ -c $<

%: %.c $(io_op) io_op.h 
	$(CC) $(DEBUG_CCFLAGS) ${LIBURING_FLAGS} -o ${build_dir}/$@ $< ${build_dir}/$(io_op) $(LDFLAGS)

clean:
	@rm -f ${build_dir}/${io_op} ${build_dir}/$(all_targets)
	@rm -f ${build_dir}/tcp_*


