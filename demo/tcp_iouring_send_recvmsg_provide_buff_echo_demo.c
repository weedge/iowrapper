#include "io_op.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s port \n", argv[0]);
        exit(1);
    }

    char buffer[MAX_MESSAGE_LEN];
    memset(buffer, 0, sizeof(buffer));

    // initialize io_uring
    struct io_uring_params params;
    struct io_uring ring;
    memset(&params, 0, sizeof(params));
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

    int group_id = 1336;
    provide_buffers(&ring, group_id);

    int sockfd = create_server_socket(atoi(argv[1]));
    if (sockfd == -1) {
        fprintf(stderr, "Failed to create server socket\n");
        exit(1);
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    add_accept(&ring, sockfd, (struct sockaddr *)&client_addr, &client_len, 0);

    // start event loop
    while (1) {
        /*
                int ret = io_uring_submit_and_wait(&ring, 1);
                if (ret < 0) {
                    perror("Error io_uring_wait_cqe\n");
                    exit(1);
                }
        */

        struct io_uring_cqe *cqe;
        io_uring_submit(&ring);
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            perror("Error io_uring_wait_cqe\n");
            exit(1);
        }

        // struct io_uring_cqe *cqes[SOMAXCONN];
        // int cqe_count = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes) / sizeof(cqes[0]));

        struct conn_info conn_i;
        memcpy(&conn_i, &cqe->user_data, sizeof(conn_i));
        int type = conn_i.type;
        switch (type) {
            case ACCEPT:
                int sock_conn_fd = cqe->res;
                io_uring_cqe_seen(&ring, cqe);
                if (sock_conn_fd < 0) {
                    fprintf(stderr, "connect failed: conn_fd %d \n", sock_conn_fd);
                    break;
                }
                printf("Accepted new connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                add_accept(&ring, sockfd, (struct sockaddr *)&client_addr, &client_len, 0);
                add_recvmsg_from_group_buff(&ring, sock_conn_fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
                break;
            case READ:
                int bytes_received = cqe->res;
                int bid = cqe->flags >> 16;
                io_uring_cqe_seen(&ring, cqe);
                if (bytes_received <= 0 || bytes_received == -ENOBUFS || bytes_received == -EMSGSIZE) {
                    fprintf(stderr, "read failed: conn_fd %d bytes_received %d\n", conn_i.fd, bytes_received);
                    // read failed, re-add buff
                    add_provide_buf(&ring, bid, group_id);
                    // no bytes available on socket, client must be disconnected
                    shutdown(sockfd, SHUT_RDWR);
                    close(conn_i.fd);
                    break;
                }
                printf("Received %d bytes from client %s:%d\n", bytes_received, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                add_sendmsg_from_group_buff(&ring, conn_i.fd, bid, bytes_received, 0);
                break;
            case WRITE:
                // write has been completed, first re-add the buffer
                add_provide_buf(&ring, conn_i.bid, group_id);

                int bytes_sent = cqe->res;
                io_uring_cqe_seen(&ring, cqe);
                if (bytes_sent <= 0) {
                    fprintf(stderr, "write failed: conn_fd %d bytes_sent %d\n", conn_i.fd, bytes_sent);
                    // no bytes available on socket, client must be disconnected
                    shutdown(sockfd, SHUT_RDWR);
                    close(conn_i.fd);
                    break;
                }
                printf("Echoed %d bytes to client %s:%d\n", bytes_sent, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                add_recvmsg_from_group_buff(&ring, sock_conn_fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
                break;
            case PROV_BUF:
                if (cqe->res < 0) {
                    fprintf(stderr, "prov_buf failed, errNO %d\n", cqe->res);
                    break;
                }
            default:
                io_uring_cqe_seen(&ring, cqe);
                fprintf(stderr, "unsupport event type %d\n", type);
                break;
        }  // end switch

    }  // end while
}
