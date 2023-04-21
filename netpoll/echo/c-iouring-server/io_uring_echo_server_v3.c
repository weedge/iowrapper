/* Echo server using io_uring multi-shot feature. */

#include <errno.h>
#include <fcntl.h>
#include <liburing.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 8192
#define IORING_FEAT_FAST_POLL (1U << 5)

static void add_accept(struct io_uring* ring, int fd, struct sockaddr* client_addr,
                       socklen_t* client_len, unsigned flags);
static void add_poll_read(struct io_uring* ring, int fd, size_t size, int is_poll);
static void add_socket_write(struct io_uring* ring, int fd, size_t size, unsigned flags);

static struct io_uring_params params;
static struct io_uring ring;
static int portno;
static int registerfiles;
static int* files;
static int* registered_files;

static int max_connections = 4092;
static int msg_len = 2046;

enum {
    ACCEPT,
    POLL_LISTEN,
    POLL_NEW_CONNECTION,
    READ,
    WRITE,
    POLL_IN_READY,
};

typedef struct conn_info {
    unsigned fd;
    unsigned type;
} conn_info;

static int init_registerfiles(int conns) {
    int i, ret;

    printf("number of registered files: %d\n", conns);
    files = calloc(conns, sizeof(int));
    if (!files) {
        fprintf(stderr, "calloc for registered files failed\n");
        return 1;
    }

    for (i = 0; i < conns; i++)
        files[i] = -1;

    registered_files = calloc(conns, sizeof(int));
    if (!registered_files) {
        fprintf(stderr, "calloc failed\n");
        return 1;
    }

    for (i = 0; i < conns; i++)
        registered_files[i] = -1;

    ret = io_uring_register_files(&ring, files, conns);
    if (ret < 0) {
        fprintf(stderr, "%s: register %d\n", __FUNCTION__, ret);
        return ret;
    }
    return 0;
}

static char* myprog;
static conn_info* conns_read;
static conn_info* conns_write;
static conn_info* conns_poll;
static char** bufs;

static void usage(void) {
    printf("Usage: %s -h   or\n", myprog);
    printf("       %s [-p port][-f][-n connections][-l msglen]\n", myprog);
    printf("   -p port		set network port\n");
    printf("   -f			enable fixed file feature\n");
    printf("   -n connections	number of network connections to establish\n");
    printf("   -l msglen		message length\n");
}

static void* submitter_fn(void* data) {
    sleep(1000);
    return NULL;
}

int main(int argc, char* argv[]) {
    int ret, i, c;
    int sock_listen_fd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    const int val = 1;
    const char* opts = "p:fn:l:h";
    int clients = 0;

    myprog = argv[0];
    while ((c = getopt(argc, argv, opts)) != -1) {
        switch (c) {
            case 'p':
                portno = atoi(optarg);
                break;
            case 'f':
                registerfiles = 1;
                break;
            case 'n':
                max_connections = atoi(optarg);
                break;
            case 'l':
                msg_len = atoi(optarg);
                break;
            case 'h':
                usage();
                exit(0);
            default:
                usage();
                exit(1);
        }
    }

    if (!portno || !max_connections) {
        int port = atoi(argv[1]);
        if (!portno && port > 3000) {
            portno = port;
        } else {
            usage();
            exit(1);
        }
    }

    if (!msg_len)
        msg_len = 128;

    printf("number of connections: %d\n", max_connections);
    printf("msg_len: %d\n", msg_len);

    max_connections += 32;
    conns_read = calloc(sizeof(struct conn_info), max_connections);
    if (!conns_read) {
        fprintf(stderr, "allocate conns failed");
        exit(1);
    }

    conns_write = calloc(sizeof(struct conn_info), max_connections);
    if (!conns_write) {
        fprintf(stderr, "allocate conns failed");
        exit(1);
    }

    conns_poll = calloc(sizeof(struct conn_info), max_connections);
    if (!conns_poll) {
        fprintf(stderr, "allocate conns failed");
        exit(1);
    }

    bufs = calloc(sizeof(char*), max_connections);
    for (i = 0; i < max_connections; i++) {
        bufs[i] = malloc(msg_len);
        if (!bufs[i]) {
            fprintf(stderr, "malloc buf failed\n");
            exit(1);
        }
    }

    sock_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sock_listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sock_listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        perror("Error binding socket..\n");
        exit(1);
    }
    ret = listen(sock_listen_fd, BACKLOG);
    if (ret < 0) {
        perror("Error listening..\n");
        exit(1);
    }
    printf("io_uring echo server listening for connections on port: %d\n", portno);

    memset(&params, 0, sizeof(params));
    params.flags = 1 << 8 | 1 << 12 | 1 << 13;
    if (io_uring_queue_init_params(max_connections, &ring, &params) < 0) {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    if (!(params.features & IORING_FEAT_FAST_POLL)) {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }

    if (registerfiles) {
        ret = init_registerfiles(max_connections);
        if (ret)
            return ret;
    }

    if (registerfiles) {
        ret = io_uring_register_files_update(&ring, sock_listen_fd, &sock_listen_fd, 1);
        if (ret < 0) {
            fprintf(stderr,
                    "lege io_uring_register_files_update "
                    "failed: %d %d\n",
                    sock_listen_fd, ret);
            exit(1);
        }
        registered_files[sock_listen_fd] = sock_listen_fd;
    }

    // add first accept sqe to monitor for new incoming connections
    add_accept(&ring, sock_listen_fd, (struct sockaddr*)&client_addr, &client_len, 0);

    pthread_t thread;
    pthread_create(&thread, NULL, submitter_fn, NULL);
    while (1) {
        int cqe_count;
        struct io_uring_cqe* cqes[BACKLOG];

        ret = io_uring_submit_and_wait(&ring, 1);
        if (ret < 0) {
            perror("Error io_uring_wait_cqe\n");
            exit(1);
        }

        cqe_count = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes) / sizeof(cqes[0]));
        // if (cqe_count < 1)
        // printf("haha %d\n", cqe_count);
        for (i = 0; i < cqe_count; ++i) {
            struct io_uring_cqe* cqe = cqes[i];
            struct conn_info* user_data = (struct conn_info*)io_uring_cqe_get_data(cqe);
            int type = user_data->type;

            if (type == ACCEPT) {
                int sock_conn_fd = cqe->res;

                io_uring_cqe_seen(&ring, cqe);

                if (registerfiles && registered_files[sock_conn_fd] == -1) {
                    ret = io_uring_register_files_update(&ring, sock_conn_fd, &sock_conn_fd, 1);
                    if (ret < 0) {
                        fprintf(stderr,
                                "io_uring_register_files_update "
                                "failed: %d %d\n",
                                sock_conn_fd, ret);
                        exit(1);
                    }
                    registered_files[sock_conn_fd] = sock_conn_fd;
                }
                /*
                 * new connected client; read data from socket
                 * and re-add accept to monitor for new
                 * connections
                 */
                // add_socket_read(&ring, sock_conn_fd, msg_len, 0);
                add_poll_read(&ring, sock_conn_fd, msg_len, 1);

                add_accept(&ring, sock_listen_fd, (struct sockaddr*)&client_addr, &client_len, 0);
                clients++;
            } else if (type == READ) {
                int bytes_read = cqe->res;

                if (bytes_read <= 0) {
                    // no bytes available on socket, client must be disconnected
                    io_uring_cqe_seen(&ring, cqe);
                    shutdown(user_data->fd, SHUT_RDWR);
                    close(user_data->fd);
                } else {
                    // bytes have been read into bufs, now add write to socket sqe
                    // printf("read event\n");
                    io_uring_cqe_seen(&ring, cqe);
                    add_socket_write(&ring, user_data->fd, bytes_read, 0);
                }
            } else if (type == WRITE) {
                // write to socket completed, re-add socket read
                // printf("write event\n");
                io_uring_cqe_seen(&ring, cqe);
                // add_socket_read(&ring, userl_data->fd, msg_len, 0);
            } else if (type == POLL_IN_READY) {
                // printf("poll in event\n");
                add_poll_read(&ring, user_data->fd, msg_len, 0);
                io_uring_cqe_seen(&ring, cqe);
            } else {
                printf("jjjjjjjjjjjjjjjjjjjjjj\n");
            }
        }
    }
}

static void add_accept(struct io_uring* ring, int fd, struct sockaddr* client_addr,
                       socklen_t* client_len, unsigned flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    conn_info* conn_i = &conns_read[fd];

    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);
    io_uring_sqe_set_flags(sqe, flags);
    if (registerfiles)
        sqe->flags |= IOSQE_FIXED_FILE;

    conn_i->fd = fd;
    conn_i->type = ACCEPT;
    io_uring_sqe_set_data(sqe, conn_i);
}

static void add_poll_read(struct io_uring* ring, int fd, size_t size, int is_poll) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);

    if (is_poll) {
        conn_info* conn_i = &conns_poll[fd];

        io_uring_prep_poll_add(sqe, fd, POLLIN);
        sqe->len = 1;
        conn_i->fd = fd;
        conn_i->type = POLL_IN_READY;
        io_uring_sqe_set_data(sqe, conn_i);
    } else {
        conn_info* conn_i = &conns_read[fd];

        io_uring_prep_recv(sqe, fd, bufs[fd], size, 0);
        io_uring_sqe_set_flags(sqe, 0);
        if (registerfiles)
            sqe->flags |= IOSQE_FIXED_FILE;

        conn_i->fd = fd;
        conn_i->type = READ;
        io_uring_sqe_set_data(sqe, conn_i);
    }
}

static void add_socket_write(struct io_uring* ring, int fd, size_t size, unsigned flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    conn_info* conn_i = &conns_write[fd];

    io_uring_prep_send(sqe, fd, bufs[fd], size, 0);
    io_uring_sqe_set_flags(sqe, flags);
    if (registerfiles) {
        // sqe->flags |= IOSQE_FIXED_FILE | IOSQE_CQE_SKIP_SUCCESS;
        sqe->flags |= IOSQE_FIXED_FILE;
    }

    conn_i->fd = fd;
    conn_i->type = WRITE;
    io_uring_sqe_set_data(sqe, conn_i);
}