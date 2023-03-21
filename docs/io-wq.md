### io-wq: provide a way to limit max number of workers
io-wq divides work into two categories:

1) Work that completes in a bounded time, like reading from a regular file
   or a block device. This type of work is limited based on the size of
   the SQ ring.

2) Work that may never complete, we call this unbounded work. The amount
   of workers here is just limited by RLIMIT_NPROC.

For various uses cases, it's handy to have the kernel limit the maximum
amount of pending workers for both categories. Provide a way to do with
with a new IORING_REGISTER_IOWQ_MAX_WORKERS operation.

IORING_REGISTER_IOWQ_MAX_WORKERS takes an array of two integers and sets
the max worker count to what is being passed in for each category. The
old values are returned into that same array. If 0 is being passed in for
either category, it simply returns the current value.

The value is capped at RLIMIT_NPROC. This actually isn't that important
as it's more of a hint, if we're exceeding the value then our attempt
to fork a new worker will fail. This happens naturally already if more
than one node is in the system, as these values are per-node internally
for io-wq.

## reference: 
1. [linux kernel commit](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=2e480058ddc21ec53a10e8b41623e245e908bdbc)

2. man io_uring_register [IORING_REGISTER_IOWQ_MAX_WORKERS](https://manpages.debian.org/unstable/liburing-dev/io_uring_register.2.en.html#IORING_REGISTER_IOWQ_MAX_WORKERS)

3. https://blog.cloudflare.com/missing-manuals-io_uring-worker-pool/