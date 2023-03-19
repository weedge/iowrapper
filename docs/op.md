### OP
block IO like readv
```c
    /*
     * The readv() call will block until all iovec buffers are filled with
     * file data. Once it returns, we should be able to access the file data
     * from the iovecs and print them on the console.
     * */
    int ret = readv(file_fd, iovecs, blocks);
    // do something
```
change to async IO by io_uring liburing
```c
    /* Initialize io_uring */
    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);



    // product io sqes to sq
    /* Get an SQE */
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    /* Setup a readv operation */
    io_uring_prep_readv(sqe, file_fd, fi->iovecs, blocks, 0);
    /* Set user data */
    io_uring_sqe_set_data(sqe, fi);
    /* Finally, submit the request */
    io_uring_submit(ring);



    // consume io cqes from cq
    /* Wait complated cqe */
    int ret = io_uring_wait_cqe(ring, &cqe);
    /* Get data from use_data*/
    struct file_info *fi = io_uring_cqe_get_data(cqe);

    // do something

    // commit consumed head
    /* Dequeue from the CQ queue; Cq ring head next cqe is seen for kernel*/
    io_uring_cqe_seen(ring, cqe);
    // or batch commit consumed head
    io_uring_cq_advance(ring,cqes)



    /* Call the clean-up function. */
    io_uring_queue_exit(&ring);
```

other op change to async IO by io_uring, have the same way, like this `io_uring_prep_*（eg：io_uring_prep_writev、io_uring_prep_accept） `


Hello World
```c
#include <liburing.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

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
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_write(sqe, STDOUT_FILENO, hello, 13, 0);

    // Tell io_uring about new SQEs in SQ.
    io_uring_submit(&ring);
    
    // Wait for a new CQE to appear in CQ.
    struct io_uring_cqe *cqe;
    ret = io_uring_wait_cqe(&ring, &cqe);
    assert(ret == 0);

    // Check for errors.
    assert(cqe->res > 0);

    // Dequeue from the CQ queue.
    io_uring_cqe_seen(&ring, cqe);

    io_uring_queue_exit(&ring);

    return 0;
}
```