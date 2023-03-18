# io_uring
![io_uring](./docs/io_uring_logo.png)
## syscall
1. man syscall or know more: [**<u>linux-insides</u>** SysCall](https://github.com/0xAX/linux-insides/tree/master/SysCall) , [linux-insides-zh SysCall](https://github.com/MintCN/linux-insides-zh/tree/master/SysCall)

2. syscall interface (liburing interface wrapper syscall for cpu arch, man liburing help doc learn more):<br>
```c
/*
 * System calls
 */
extern int io_uring_setup(unsigned entries, struct io_uring_params *p);
extern int io_uring_enter(int fd, unsigned to_submit,
	unsigned min_complete, unsigned flags, sigset_t *sig);
extern int io_uring_register(int fd, unsigned int opcode, void *arg,
	unsigned int nr_args);
```
3. linux kernel v6.3-rc2 interface ( [syscall](https://sourcegraph.com/github.com/torvalds/linux@v6.3-rc2/-/blob/tools/io_uring/syscall.c) )
    * io_uring_setup -> syscall  --(soft interrupt)-->> sys_io_uring_setup -> [SYSCALL_DEFINE](https://sourcegraph.com/github.com/torvalds/linux@v6.3-rc2/-/blob/include/linux/syscalls.h?L226)2() > [sysreturn io_uring_setup](https://sourcegraph.com/github.com/torvalds/linux@v6.3-rc2/-/blob/io_uring/io_uring.c?L3828)
    * io_uring_enter -> syscall --(soft interrupt)-->> sys_io_uring_enter ->  [SYSCALL_DEFINE6(sysreturn io_uring_enter)](https://sourcegraph.com/github.com/torvalds/linux@v6.3-rc2/-/blob/io_uring/io_uring.c?L3392)
    * io_uring_register -> syscall --(soft interrupt)-->> sys_io_uring_register -> [SYSCALL_DEFINE4(io_uring_register)](https://sourcegraph.com/github.com/torvalds/linux@v6.3-rc2/-/blob/io_uring/io_uring.c?L4303)

4. see more support [**io_uring_op**](https://sourcegraph.com/github.com/torvalds/linux@v6.3-rc2/-/blob/include/uapi/linux/io_uring.h?L176)


## liburing
1. use liburing see readme, more detail: https://github.com/axboe/liburing <br> use cgo Notice: https://dave.cheney.net/2016/01/18/cgo-is-not-go <br> io_uring want learn more see those [updating]:
    * **https://kernel.dk/io_uring.pdf**
    * https://www.youtube.com/watch?v=-5T4Cjw46ys
    * **https://kernel.dk/axboe-kr2022.pdf**
    * https://kernel-recipes.org/en/2022/whats-new-with-io_uring/

```shell
# see linux os kernel support uring syscall
cat /proc/kallsyms | grep uring
# when use io_uring check sq
ps --ppid ${pid} | grep io_uring-sq
```

2. if need use golang runtime native support, please see this Note: [#31908](https://github.com/golang/go/issues/31908)

3. 3rd io_uring support for golang:
    * https://github.com/hodgesds/iouring-go 
    * https://github.com/godzie44/go-uring 

4. RocksDB MultiGet use IO Uring interface: https://github.com/facebook/rocksdb/wiki/MultiGet-Performance

## bench scene (net/storage IO)
1. net IO for netpoll scenes
    * tcp echo server, build & bench:
    ```shell
    make build-echo
    make bench-echo
    ```
2. storage IO for data file storage in HDD, NVMe SSD etc hardware scenes

## learn more try to change io
1. badger: https://dgraph.io/blog/post/badger/
2. pebble: https://www.cockroachlabs.com/blog/pebble-rocksdb-kv-store/


## linux kernel for new io_uring feature
### compiling linux kernel for develop io_uring-** tag branch
1. [kernel_compile](https://www.cyberciti.biz/tips/compiling-linux-kernel-26.html)
2. [the linux kernel archives](https://www.kernel.org/)
### upgrade release linux kernel for ubuntu 
eg: upgrage linux kernel to v6.3-rc2 for Ubuntu 22.04.2 LTS
```shell
sudo apt-get upgrade
sudo apt-get update
# mainline: https://kernel.ubuntu.com/~kernel-ppa/mainline/?C=N;O=D ; u can chose latest linux kernel
sudo wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v6.3-rc2/amd64/linux-headers-6.3.0-060300rc2-generic_6.3.0-060300rc2.202303122031_amd64.deb
sudo wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v6.3-rc2/amd64/linux-headers-6.3.0-060300rc2_6.3.0-060300rc2.202303122031_all.deb
sudo wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v6.3-rc2/amd64/linux-image-unsigned-6.3.0-060300rc2-generic_6.3.0-060300rc2.202303122031_amd64.deb
sudo wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v6.3-rc2/amd64/linux-modules-6.3.0-060300rc2-generic_6.3.0-060300rc2.202303122031_amd64.deb
sudo apt install ./linux-*.deb
#restart
sudo reboot
```
Develop Tips: 
* use UTM/VirtualBox/VMware install VM to run;
* use docker container to run;
* use vscode ssh remote or devcontainer to develop; recomend dev container with local env;
* bench net IO , server run in VM or physical machine;
* bench storage IO , server run in physical machine mount HDD, NVMe SSD etc hardware device;

Bench Tips:
* Bench net IO, use tcp tools: 
    * tcpdump check RST;
    * netstat check es,cw,tw stat and static tcp send recv;
    * ss static tcp stat;
* Bench storage IO, use storage IO tools: 
    * fio bench ioengine, check IOPS;
    * vmstat check io bi,bo, swap;
    * iostat check device r/w tps, iowait;
```shell
sudo apt install sysstat
```

## reference
1. **https://unixism.net/loti/**
2. https://unixism.net/2020/04/io-uring-by-example-article-series/
3. windows IORing: https://windows-internals.com/ioring-vs-io_uring-a-comparison-of-windows-and-linux-implementations/ 
4. [Diego Didona - **<u>Understanding Modern Storage APIs: A systematic study of libaio, SPDK, and io_uring</u>**](https://atlarge-research.com/pdfs/2022-systor-apis.pdf) , [video](https://www.youtube.com/watch?v=5jKKVdJJqKY)
4. [awesome-iouring](https://github.com/espoal/awesome-iouring)

