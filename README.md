## io_uring
1. use liburing see readme, more detail: https://github.com/axboe/liburing <br> use cgo Notice: https://dave.cheney.net/2016/01/18/cgo-is-not-go

```c
/*
 * io_uring want learn more see:
 * 1. https://github.com/axboe/liburing
 * 2. https://www.youtube.com/watch?v=-5T4Cjw46ys
 * 3. https://kernel-recipes.org/en/2022/whats-new-with-io_uring/
 * 4. https://lore.kernel.org/io-uring/
 *
 */
```
```shell
# see linux os kernel support uring syscall
cat /proc/kallsyms | grep uring
# when use io_uring check sq
ps --ppid ${pid} | grep io_uring-sq
```


2. u need use golang runtime native support, please Note: [#31908](https://github.com/golang/go/issues/31908)

3. 3rd io_uring support for golang https://github.com/hodgesds/iouring-go  https://github.com/godzie44/go-uring 

4. RocksDB MultiGet use IO Uring interface: https://github.com/facebook/rocksdb/wiki/MultiGet-Performance



### learn more try to change io
1. badger: https://dgraph.io/blog/post/badger/
2. pebble: https://www.cockroachlabs.com/blog/pebble-rocksdb-kv-store/


### compiling linux kernel for new io_uring feature
1. [kernel_compile](https://www.cyberciti.biz/tips/compiling-linux-kernel-26.html)
2. [the linux kernel archives](https://www.kernel.org/)



### reference
1. https://unixism.net/loti/
2. https://unixism.net/2020/04/io-uring-by-example-article-series/
3. windows IORing: https://windows-internals.com/ioring-vs-io_uring-a-comparison-of-windows-and-linux-implementations/ 
