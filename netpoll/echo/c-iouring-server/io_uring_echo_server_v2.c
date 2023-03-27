/**/
#include <errno.h>
#include <fcntl.h>
#include <liburing.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
static void add_socket_read(struct io_uring* ring, int fd, size_t size, unsigned flags);
static void add_socket_write(struct io_uring* ring, int fd, int bid, size_t size, unsigned flags);

static struct io_uring_params params;
static struct io_uring ring;
static int portno;
static int registerfiles;
static int* files;
static int* registered_files;

static int max_connections = 65536;
static int msg_len = 128;

enum {
    ACCEPT,
    POLL_LISTEN,
    POLL_NEW_CONNECTION,
    READ,
    WRITE,
};

typedef struct conn_info {
    unsigned fd;
    unsigned type;
    bool multishot_recv;
    void* buf;
} conn_info;

static int init_registerfiles(void) {
    struct rlimit r;
    int i, ret;

    ret = getrlimit(RLIMIT_NOFILE, &r);
    if (ret < 0) {
        fprintf(stderr, "getrlimit: %s\n", strerror(errno));
        return ret;
    } else
        printf("RLIMIT_NOFILE: %ld %ld\n", r.rlim_cur, r.rlim_max);

    if (r.rlim_max > 32768)
        r.rlim_max = 32768;
    printf("number of registered files: %ld\n", r.rlim_max);
    files = calloc(r.rlim_max, sizeof(int));
    if (!files) {
        fprintf(stderr, "calloc for registered files failed\n");
        return 1;
    }

    for (i = 0; i < r.rlim_max; i++)
        files[i] = -1;

    registered_files = calloc(r.rlim_max, sizeof(int));
    if (!registered_files) {
        fprintf(stderr, "calloc failed\n");
        return 1;
    }

    for (i = 0; i < r.rlim_max; i++)
        registered_files[i] = -1;

    ret = io_uring_register_files(&ring, files, r.rlim_max);
    if (ret < 0) {
        fprintf(stderr, "%s: register %d\n", __FUNCTION__, ret);
        return ret;
    }
    return 0;
}

static volatile int workcount;

static void workload() {
    volatile int i = 0;

    for (i = 0; i < 1000; i++)
        workcount++;
}

static char* myprog;
static conn_info* conns;
static char** bufs;

static void usage(void) {
    printf("Usage: %s -h   or\n", myprog);
    printf("       %s [-p port][-f][-n connections][-l msglen]\n", myprog);
    printf("   -p port		set network port\n");
    printf("   -f			enable fixed file feature\n");
    printf("   -n connections	number of network connections to establish\n");
    printf("   -l msglen		message length\n");
}

int main(int argc, char* argv[]) {
    int ret, i, c;
    int sock_listen_fd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    const int val = 1;
    const char* opts = "p:fn:l:h";
    struct io_uring_buf_reg reg = {};
    size_t buf_ring_size, buf_pool_size;
    int nr_bufs;
    struct io_uring_buf_ring* br = NULL;
    void *ptr, *buf_ptr;

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

    if (!portno || !max_connections || !msg_len) {
        usage();
        exit(1);
    }

    printf("max_connections: %d\n", max_connections);

    printf("number of connections: %d\n", max_connections);
    printf("msg_len: %d\n", msg_len);

    nr_bufs = max_connections;
    nr_bufs = 1024;
    max_connections += 32;
    conns = calloc(sizeof(struct conn_info), max_connections);
    if (!conns) {
        fprintf(stderr, "allocate conns failed");
        exit(1);
    }
    memset(conns, 0, sizeof(struct conn_info) * max_connections);

    bufs = calloc(sizeof(char*), max_connections);
    for (i = 0; i < max_connections; i++) {
        bufs[i] = malloc(msg_len);
        if (!bufs[i]) {
            fprintf(stderr, "malloc buf failed\n");
            exit(1);
        }
    }

    sock_listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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
    if (io_uring_queue_init_params(32768, &ring, &params) < 0) {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    if (!(params.features & IORING_FEAT_FAST_POLL)) {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }

    buf_ring_size = sizeof(struct io_uring_buf) * nr_bufs;
    if (posix_memalign(&ptr, 4096, buf_ring_size)) {
        printf("posix_memalign failed\n");
        exit(1);
    }

    reg.ring_addr = (unsigned long)ptr;
    reg.ring_entries = nr_bufs;
    reg.bgid = 16;
    if (io_uring_register_buf_ring(&ring, &reg, 0))
        exit(1);

    buf_pool_size = msg_len * nr_bufs;
    if (posix_memalign(&buf_ptr, 4096, buf_pool_size)) {
        printf("posix_memalign failed\n");
        exit(1);
    }
    br = ptr;
    for (i = 0; i < nr_bufs; i++) {
        io_uring_buf_ring_add(br, buf_ptr + msg_len * i, msg_len, i,
                              io_uring_buf_ring_mask(nr_bufs), i);
    }
    io_uring_buf_ring_advance(br, nr_bufs);

    if (registerfiles) {
        ret = init_registerfiles();
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

    while (1) {
        int cqe_count;
        struct io_uring_cqe* cqes[BACKLOG];
        int buf_added;

        ret = io_uring_submit_and_wait(&ring, 1);
        if (ret < 0) {
            perror("Error io_uring_wait_cqe\n");
            exit(1);
        }

        // printf("submit:%d\n", ret);

        cqe_count = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes) / sizeof(cqes[0]));
        buf_added = 0;
        for (i = 0; i < cqe_count; ++i) {
            struct io_uring_cqe* cqe = cqes[i];
            struct conn_info* user_data = (struct conn_info*)io_uring_cqe_get_data(cqe);
            int type = user_data->type;

            if (type == ACCEPT) {
                int sock_conn_fd = cqe->res;

                // io_uring_cqe_seen(&ring, cqe);

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
                add_socket_read(&ring, sock_conn_fd, msg_len, 0);
                add_accept(&ring, sock_listen_fd, (struct sockaddr*)&client_addr, &client_len, 0);
                // printf("lege add add_socket_read\n");
            } else if (type == READ) {
                int bytes_read = cqe->res;
                int buf_id;

                buf_id = cqe->flags >> IORING_CQE_BUFFER_SHIFT;

                // printf("get read len %d %d\n", bytes_read, cqe->flags & IORING_CQE_F_MORE);
                io_uring_buf_ring_add(br, buf_ptr + msg_len * buf_id, msg_len, 0,
                                      io_uring_buf_ring_mask(nr_bufs), 0);
                // io_uring_buf_ring_advance(br, 1);
                buf_added++;
                if (bytes_read <= 0) {
                    // printf("read failed:%d\n", bytes_read);
                    //   no bytes available on socket, client must be disconnected
                    //   io_uring_cqe_seen(&ring, cqe);
                    shutdown(user_data->fd, SHUT_RDWR);
                    close(user_data->fd);
                } else {
                    // bytes have been read into bufs, now add write to socket sqe
                    // io_uring_cqe_seen(&ring, cqe);
                    // printf("add write\n");
                    add_socket_write(&ring, user_data->fd, buf_id, bytes_read, 0);
                }
                // workload();
            } else if (type == WRITE) {
                // printf("write success len: %d \n", cqe->res);
                //   write to socket completed, re-add socket read
                //   io_uring_cqe_seen(&ring, cqe);
                add_socket_read(&ring, user_data->fd, msg_len, 0);
                // user_data->type = READ;
            }
        }
        io_uring_buf_ring_advance(br, buf_added);
        io_uring_cq_advance(&ring, cqe_count);
    }
}

static void add_accept(struct io_uring* ring, int fd, struct sockaddr* client_addr,
                       socklen_t* client_len, unsigned flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    conn_info* conn_i = &conns[fd];

    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);
    io_uring_sqe_set_flags(sqe, flags);
    if (registerfiles)
        sqe->flags |= IOSQE_FIXED_FILE;

    conn_i->fd = fd;
    conn_i->type = ACCEPT;
    io_uring_sqe_set_data(sqe, conn_i);
}

static void add_socket_read(struct io_uring* ring, int fd, size_t size, unsigned flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    conn_info* conn_i = &conns[fd];

    io_uring_prep_recv(sqe, fd, NULL, size, 0);
    // io_uring_prep_recv_multishot(sqe, fd, NULL, 0, 0);
    sqe->flags |= IOSQE_BUFFER_SELECT;
    sqe->buf_group = 16;
    // io_uring_sqe_set_flags(sqe, flags);
    if (registerfiles)
        sqe->flags |= IOSQE_FIXED_FILE;

    conn_i->fd = fd;
    conn_i->type = READ;
    conn_i->multishot_recv = true;
    io_uring_sqe_set_data(sqe, conn_i);
}

static void add_socket_write(struct io_uring* ring, int fd, int bid, size_t size, unsigned flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    conn_info* conn_i = &conns[fd];

    // send(fd, bufs[fd], size, 0);
    // io_uring_prep_send(sqe, fd, bufs[fd], size, 0);
    io_uring_prep_send(sqe, fd, &bufs[bid], size, 0);
    io_uring_sqe_set_flags(sqe, flags);
    if (registerfiles)
        sqe->flags |= IOSQE_FIXED_FILE;

    conn_i->fd = fd;
    conn_i->type = WRITE;
    io_uring_sqe_set_data(sqe, conn_i);
}