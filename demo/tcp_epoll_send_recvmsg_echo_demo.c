#include "io_op.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(1);
    }

    char buffer[MAX_MESSAGE_LEN];
    memset(buffer, 0, sizeof(buffer));

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
                int sock_conn_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addrlen);
                if (sock_conn_fd == -1) {
                    error("error");
                }
                printf("Accepted new connection from %s:%d\n", inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port));
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
                int bytes_received = recvmsg(newsockfd, &msg, 0);
                // int bytes_received = recv(newsockfd, buffer, MAX_MESSAGE_LEN, 0);
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

                    int bytes_sent = sendmsg(newsockfd, &msg, 0);
                    if (bytes_sent < 0) {
                        perror("Failed to send data to client");
                        close(newsockfd);
                        continue;
                    }
                    printf("Echoed %d bytes to client %s:%d\n", bytes_sent,
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                }
            }
        }
    }
    close(sockfd);
}