/* SPDX-License-Identifier: MIT */

#ifndef LIBURING_IO_WRAPPER_DEMO_OP_H
#define LIBURING_IO_WRAPPER_DEMO_OP_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "liburing.h"

#define IORING_MAX_ENTRIES 4096
#define MAX_EVENTS 10
#define MAX_MESSAGE_LEN 1024
#define MAX_CONNECTIONS 4096
#define BUFFERS_COUNT MAX_CONNECTIONS

static inline void error(char *msg) {
    perror(msg);
    printf("erreur...\n");
    exit(1);
}

enum {
    NOP,
    ACCEPT,
    READ,
    WRITE,
    PROV_BUF,
};

typedef struct conn_info {
    __u32 fd;
    __u16 type;
    __u16 bid;
} conn_info;

int create_server_socket(int port);

void add_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr,
                socklen_t *client_len, unsigned flags);

void add_recvmsg(struct io_uring *ring, int fd, char buff[],
                 size_t buff_size, unsigned flags);

// set recvmsg to ring sqe, used buff from a registed group provide buff pool
void add_recvmsg_from_group_buff(struct io_uring *ring, int fd, unsigned gid,
                                 size_t buff_size, unsigned flags);

void add_sendmsg(struct io_uring *ring, int fd, char *buff,
                 size_t msg_size, unsigned flags);

// set sendmsg to ring sqe, used index bid buf from a registed group provide buffers pool;
// set bid for user_data, re-add index bid buff after op complated.
void add_sendmsg_from_group_buff(struct io_uring *ring, int fd, __u16 bid,
                                 size_t msg_size, unsigned flags);

// register a group buffers for buffer selection
void provide_buffers(struct io_uring *ring, int group_id);

// add one bid provide buff for a registed group buffers pool, like buff pool put
void add_provide_buf(struct io_uring *ring, __u16 bid, unsigned gid);

void add_recv_from_group_buff(struct io_uring *ring, int fd, unsigned gid,
                              size_t message_size, unsigned flags);

void add_send_from_group_buff(struct io_uring *ring, int fd, __u16 bid,
                              size_t message_size, unsigned flags);
#endif /* #ifndef LIBURING_IO_WRAPPER_DEMO_OP_H */