## Reading a completion queue entry
As always, we take up the completion side of things first since it is simpler than its submission counterpart. These explanations are even required because we need to discuss memory ordering and how we need to deal with it. Otherwise, we just want to see how to deal with ring buffers. For completion events, the kernel adds CQEs to the ring buffer and updates the tail, while we read from the head in user space. As in any ring buffer, if the head and the tail are equal, it means the ring buffer is empty. Take a look at the code below:

```c
unsigned head;
head = cqring->head;
read_barrier(); /* ensure previous writes are visible */
if (head != cqring->tail) {
    /* There is data available in the ring buffer */
    struct io_uring_cqe *cqe;
    unsigned index;
    index = head & (cqring->mask);
    cqe = &cqring->cqes[index];
    /* process completed cqe here */
     ...
    /* we've now consumed this entry */
    head++;
}
cqring->head = head;
write_barrier();
```
To get the index of the head, the application needs to mask head with the size mask of the ring buffer. Remember that any line in the code above could be running after a context switch. So, right before the comparison, we have a read_barrier() so that, if the kernel has indeed updated the tail, we can read it as part of our comparison in the if statement. Once we get the CQE and process it, we update the head letting the kernel know that we’ve consumed an entry from the ring buffer. The final write_barrier() ensures that writes we do become visible so that the kernel knows about it.

---

## Making a submission
Making a submission is the opposite of reading a completion. While dealing with completion the kernel added entries to the tail and we read entries off the head of the ring buffer, when making a submission, we add to the tail and kernel reads entries off the head of the submission ring buffer.

```c
struct io_uring_sqe *sqe;
unsigned tail, index;
tail = sqring->tail;
index = tail & (*sqring->ring_mask);
sqe = &sqring->sqes[index];
/* this function call fills in the SQE details for this IO request */
app_init_io(sqe);
/* fill the SQE index into the SQ ring array */
sqring->array[index] = index;
tail++;
write_barrier();
sqring->tail = tail;
write_barrier();
```

In the code snippet above, the app_init_io() function in the application fills up details of the request for submission. Before the tail is updated, we have a write_barrier() to ensure that the previous writes are ordered. Then we update the tail and call write_barrier() once more to ensure that our update is seen. We’re lining up our ducks here.

## liburing barrier
[/usr/include/liburing/barrier.h](https://github.com/axboe/liburing/blob/master/src/include/liburing/barrier.h)
```c
#include <stdatomic.h>

#define IO_URING_WRITE_ONCE(var, val)				\
	atomic_store_explicit((_Atomic __typeof__(var) *)&(var),	\
			      (val), memory_order_relaxed)
#define IO_URING_READ_ONCE(var)					\
	atomic_load_explicit((_Atomic __typeof__(var) *)&(var),	\
			     memory_order_relaxed)

#define io_uring_smp_store_release(p, v)			\
	atomic_store_explicit((_Atomic __typeof__(*(p)) *)(p), (v), \
			      memory_order_release)
#define io_uring_smp_load_acquire(p)				\
	atomic_load_explicit((_Atomic __typeof__(*(p)) *)(p),	\
			     memory_order_acquire)

#define io_uring_smp_mb()					\
	atomic_thread_fence(memory_order_seq_cst)
```