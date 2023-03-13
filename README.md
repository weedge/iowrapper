### io_uring
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


2. u need use golang runtime native support, please Note: [#31908](https://github.com/golang/go/issues/31908)

3. 3th io_uring support for golang https://github.com/hodgesds/iouring-go  https://github.com/godzie44/go-uring 

4. RocksDB MultiGet use IO Uring interface: https://github.com/facebook/rocksdb/wiki/MultiGet-Performance



### learn more try to change io
1. badger: https://dgraph.io/blog/post/badger/
2. pebble: https://www.cockroachlabs.com/blog/pebble-rocksdb-kv-store/