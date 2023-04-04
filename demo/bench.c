/* SPDX-License-Identifier: MIT */

/*
 gcc -Wall -O3 -D_GNU_SOURCE -o ../build/bench bench.c ../iouring/liburing/src/liburing.a
 sudo perf stat -e io_uring:* ../build/bench
 sudo perf stat -a ../build/bench
*/
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "liburing.h"

static char buf[4096];
static unsigned long runtime_ms = 10000;

static unsigned long gettimeofday_ms(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int bench_do(struct io_uring ring, unsigned long* nr_reqs) {
    unsigned long tstop;
    struct io_uring_cqe* cqe;
    struct io_uring_sqe* sqe;
    int ret, i, qd = 32;
    int pipe1[2];

    if (pipe(pipe1) != 0) {
        perror("pipe");
        return 1;
    }

    ret = io_uring_register_files(&ring, pipe1, 2);
    if (ret < 0) {
        fprintf(stderr, "io_uring_register_files failed\n");
        return 1;
    }

    tstop = gettimeofday_ms() + runtime_ms;
    do {
        for (i = 0; i < qd; i++) {
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_poll_add(sqe, 0, POLLIN);
            sqe->flags |= IOSQE_FIXED_FILE;
            sqe->user_data = 1;
        }

        ret = io_uring_submit(&ring);
        if (ret != qd) {
            fprintf(stderr, "child: sqe submit failed: %d\n", ret);
            return 1;
        }

        ret = write(pipe1[1], buf, 1);
        if (ret != 1) {
            fprintf(stderr, "write failed %i\n", errno);
            return 1;
        }
        ret = read(pipe1[0], buf, 1);
        if (ret != 1) {
            fprintf(stderr, "read failed %i\n", errno);
            return 1;
        }

        for (i = 0; i < qd; i++) {
            ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0) {
                fprintf(stderr, "child: wait completion %d\n", ret);
                break;
            }
            io_uring_cqe_seen(&ring, cqe);
            (*nr_reqs)++;
        }
    } while (gettimeofday_ms() < tstop);

    return 0;
}

int main(void) {
    unsigned long nr_reqs = 0;
    struct io_uring ring;
    int ret;

    ret = io_uring_queue_init(1024, &ring, IORING_SETUP_SINGLE_ISSUER);
    if (ret == -EINVAL) {
        fprintf(stderr, "can't single\n");
        ret = io_uring_queue_init(1024, &ring, 0);
    }
    if (ret) {
        fprintf(stderr, "child: ring setup failed: %d\n", ret);
        return 1;
    }

    ret = io_uring_register_ring_fd(&ring);
    if (ret < 0) {
        fprintf(stderr, "io_uring_register_ring_fd failed\n");
        return 1;
    }

    ret = bench_do(ring, &nr_reqs);
    if (ret == 1) {
        return 1;
    }

    fprintf(stderr, "requests/s: %lu\n", nr_reqs * 1000UL / runtime_ms);

    return 0;
}
