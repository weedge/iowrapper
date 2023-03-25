/* SPDX-License-Identifier: MIT */

#include "io_op.h"

char bufs[BUFFERS_COUNT][MAX_MESSAGE_LEN] = {0};

int create_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "bind failed\n");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, SOMAXCONN) == -1) {
        fprintf(stderr, "listen failed\n");
        close(sockfd);
        return -1;
    }

    printf("echo server listening for connections on port: %d\n", port);
    return sockfd;
}

void add_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr,
                socklen_t *client_len, unsigned flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = ACCEPT,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_recvmsg(struct io_uring *ring, int fd, char buff[],
                 size_t buff_size, unsigned flags) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    iov[0].iov_base = buff;
    iov[0].iov_len = buff_size;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recvmsg(sqe, fd, &msg, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = READ,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

// set recvmsg to ring sqe, used buff from a registed group provide buff pool
void add_recvmsg_from_group_buff(struct io_uring *ring, int fd, unsigned gid,
                                 size_t buff_size, unsigned flags) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    iov[0].iov_base = NULL;
    iov[0].iov_len = buff_size;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recvmsg(sqe, fd, &msg, 0);
    io_uring_sqe_set_flags(sqe, flags);
    sqe->buf_group = gid;

    conn_info conn_i = {
        .fd = fd,
        .type = READ,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_sendmsg(struct io_uring *ring, int fd, char *buff,
                 size_t msg_size, unsigned flags) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    iov[0].iov_base = buff;
    iov[0].iov_len = msg_size;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_sendmsg(sqe, fd, &msg, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = WRITE,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

// set sendmsg to ring sqe, used index bid buf from a registed group provide buffers pool;
// set bid for user_data, re-add index bid buff after op complated.
void add_sendmsg_from_group_buff(struct io_uring *ring, int fd, __u16 bid,
                                 size_t msg_size, unsigned flags) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    iov[0].iov_base = &bufs[bid];
    iov[0].iov_len = msg_size;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_sendmsg(sqe, fd, &msg, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = WRITE,
        .bid = bid,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

// register a group buffers for buffer selection
void provide_buffers(struct io_uring *ring, int group_id) {
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    sqe = io_uring_get_sqe(ring);
    io_uring_prep_provide_buffers(sqe, bufs, MAX_MESSAGE_LEN, BUFFERS_COUNT,
                                  group_id, 0);

    io_uring_submit(ring);
    io_uring_wait_cqe(ring, &cqe);
    if (cqe->res < 0) {
        printf("cqe->res = %d\n", cqe->res);
        exit(1);
    }
    io_uring_cqe_seen(ring, cqe);
}

// add one bid provide buff for a registed group buffers pool, like buff pool put
void add_provide_buf(struct io_uring *ring, __u16 bid, unsigned gid) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_provide_buffers(sqe, bufs[bid], MAX_MESSAGE_LEN, 1, gid, bid);

    conn_info conn_i = {
        .fd = 0,
        .type = PROV_BUF,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_recv_from_group_buff(struct io_uring *ring, int fd, unsigned gid,
                              size_t message_size, unsigned flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, NULL, message_size, 0);
    io_uring_sqe_set_flags(sqe, flags);
    sqe->buf_group = gid;

    conn_info conn_i = {
        .fd = fd,
        .type = READ,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_send_from_group_buff(struct io_uring *ring, int fd, __u16 bid,
                              size_t message_size, unsigned flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_send(sqe, fd, &bufs[bid], message_size, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = WRITE,
        .bid = bid,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}