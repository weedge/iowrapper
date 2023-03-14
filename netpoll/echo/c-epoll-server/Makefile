CCFLAGS ?= -Wall -O2 -D_GNU_SOURCE
all_targets = epoll_echo_server

.PHONY: epoll_echo_server

all: $(all_targets)

clean:
	rm -f $(all_targets)

epoll_echo_server:
	$(CC) main.c -o ./epoll_echo_server ${CCFLAGS}

debug:
	$(CC) main.c -o ./epoll_echo_server ${CCFLAGS} -g