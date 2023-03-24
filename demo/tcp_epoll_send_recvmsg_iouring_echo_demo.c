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

#define MAX_EVENTS 10
#define MAX_MESSAGE_LEN 1024

void error(char *msg) {
    perror(msg);
    printf("erreur...\n");
    exit(1);
}

static int create_server_socket(int port) {
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
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, SOMAXCONN) == -1) {
        close(sockfd);
        return -1;
    }

    printf("epoll echo server listening for connections on port: %d\n", port);
    return sockfd;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(1);
    }

    char buffer[MAX_MESSAGE_LEN];
    memset(buffer, 0, sizeof(buffer));

    // initialize io_uring
    struct io_uring_params params;
    struct io_uring ring;
    memset(&params, 0, sizeof(params));
    if (argc >= 3) {
        int res = strcmp(argv[2], "sqp");
        if (res == 0) {
            printf("setup sqpoll mode\n");
            params.flags |= IORING_SETUP_SQPOLL;
            params.sq_thread_cpu = 4;
            params.sq_thread_idle = 10000;
        }
    }

    if (io_uring_queue_init_params(10240, &ring, &params) < 0) {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    // check if IORING_FEAT_FAST_POLL is supported
    if (!(params.features & IORING_FEAT_FAST_POLL)) {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }

    // check if buffer selection is supported
    // https://lore.kernel.org/io-uring/20200228203053.25023-1-axboe@kernel.dk/T/#u
    struct io_uring_probe *probe;
    probe = io_uring_get_probe_ring(&ring);
    if (!probe || !io_uring_opcode_supported(probe, IORING_OP_PROVIDE_BUFFERS)) {
        printf("Buffer select not supported, skipping...\n");
        exit(0);
    }
    // free(probe);

    int sockfd = create_server_socket(atoi(argv[1]));
    if (sockfd == -1) {
        fprintf(stderr, "Failed to create server socket\n");
        exit(1);
    }

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(1);
    }

    struct epoll_event event;
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
        perror("epoll_ctl");
        exit(1);
    }

    struct epoll_event events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    while (1) {
        int n = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            exit(1);
        }

        for (int i = 0; i < n; i++) {
            if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                close(events[i].data.fd);
                continue;
            }

            if (events[i].data.fd == sockfd) {
                // New incoming connection
                int sock_conn_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen);
                if (sock_conn_fd == -1) {
                    error("error");
                }
                printf("Accepted new connection from %s:%d\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = sock_conn_fd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_conn_fd, &event) == -1) {
                    error("Error adding new event to epoll..\n");
                }
            } else {
                int newsockfd = events[i].data.fd;
                struct msghdr msg = {0};
                struct iovec iov[1];
                iov[0].iov_base = buffer;
                iov[0].iov_len = MAX_MESSAGE_LEN - 1;
                msg.msg_iov = iov;
                msg.msg_iovlen = 1;

                struct io_uring_sqe *sqe;
                struct io_uring_cqe *cqe;
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_recvmsg(sqe, newsockfd, &msg, 0);
                io_uring_submit(&ring);
                io_uring_wait_cqe(&ring, &cqe);
                io_uring_cqe_seen(&ring, cqe);
                int bytes_received = cqe->res;

                // int bytes_received = recvmsg(newsockfd, &msg, 0);
                //  int bytes_received = recv(newsockfd, buffer, MAX_MESSAGE_LEN, 0);

                if (bytes_received <= 0) {
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, newsockfd, NULL);
                    shutdown(newsockfd, SHUT_RDWR);
                } else {
                    // Echo the received data back to the client
                    // send(newsockfd, buffer, bytes_received, 0);
                    iov[0].iov_base = buffer;
                    iov[0].iov_len = bytes_received;
                    msg.msg_iov = iov;
                    msg.msg_iovlen = 1;

                    struct io_uring_sqe *sqe;
                    struct io_uring_cqe *cqe;
                    sqe = io_uring_get_sqe(&ring);
                    io_uring_prep_sendmsg(sqe, newsockfd, &msg, 0);
                    io_uring_submit(&ring);
                    io_uring_wait_cqe(&ring, &cqe);
                    io_uring_cqe_seen(&ring, cqe);
                    int bytes_sent = cqe->res;

                    // int bytes_sent = sendmsg(newsockfd, &msg, 0);

                    if (bytes_sent < 0) {
                        perror("Failed to send data to client");
                        close(newsockfd);
                        continue;
                    }
                    printf("Echoed %d bytes to client %s:%d\n",
                           bytes_sent, inet_ntoa(client_addr.sin_addr),
                           ntohs(client_addr.sin_port));
                }
            }
        }
    }
    close(sockfd);
}
