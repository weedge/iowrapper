/*
 gcc -Wall -O3 -D_GNU_SOURCE -luring -o ../build/iouring_hellworld
 helloworld.c
*/
#include <assert.h>
#include <liburing.h>
#include <string.h>
#include <unistd.h>

int main() {
    struct io_uring_params params;
    struct io_uring ring;
    memset(&params, 0, sizeof(params));

    /**
     * Create an io_uring instance, don't use any custom options.
     * The capacity of the SQ and CQ buffer is specified as 4096 entries.
     */
    int ret = io_uring_queue_init_params(4, &ring, &params);
    assert(ret == 0);

    char hello[] = "hello world!\n";

    // Add a write operation to the SQ queue.
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_write(sqe, STDOUT_FILENO, hello, 13, 0);

    // Tell io_uring about new SQEs in SQ.
    io_uring_submit(&ring);

    // Wait for a new CQE to appear in CQ.
    struct io_uring_cqe* cqe;
    ret = io_uring_wait_cqe(&ring, &cqe);
    assert(ret == 0);

    // Check for errors.
    assert(cqe->res > 0);

    // Dequeue from the CQ queue.
    io_uring_cqe_seen(&ring, cqe);

    io_uring_queue_exit(&ring);

    return 0;
}