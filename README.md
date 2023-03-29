# io_uring
> Very quietly I take my leave,<br>
> As quietly as I came here;<br>
> Gently waving my sleeve,<br>
> I am not taking away a single cloud. - Xu Zhimo
---

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

5. io_uring kernel bug (fixed: [io_put_kbuf](https://sourcegraph.com/github.com/torvalds/linux@v6.3-rc2/-/blob/io_uring/kbuf.h?L124)):
    * [IORING_OP_PROVIDE_BUFFERS](https://yhbt.net/lore/all/20200228203053.25023-1-axboe@kernel.dk/T/)
    * http://www.aqwu.net/blog/index.php/2022/03/17/io_uring-linux/
    * https://starlabs.sg/blog/2022/06-io_uring-new-code-new-bugs-and-a-new-exploit-technique/

## liburing
1. use liburing see readme, more detail: https://github.com/axboe/liburing <br> use cgo Notice: https://dave.cheney.net/2016/01/18/cgo-is-not-go <br> io_uring want learn more see those [updating]:
    * **https://kernel.dk/io_uring.pdf**
    * https://www.youtube.com/watch?v=-5T4Cjw46ys
    * **https://kernel.dk/axboe-kr2022.pdf**
    * https://kernel-recipes.org/en/2022/whats-new-with-io_uring/

```shell
# see linux os kernel support uring kernel symbols
cat /proc/kallsyms | grep uring
#sudo bpftrace -l | grep io_uring
#sudo perf list 'io_uring:*'
#sudo perf stat -e io_uring:io_uring_submit_sqe -- timeout 1 {process}
#sudo perf stat -e io_uring:io_uring_submit_sqe --timeout 1000 -p {pid}
#sudo perf stat -a -d -d -d --timeout 1000 -p {pid}

# when use io_uring check iou-***-*** kernel thread {wrk(work), sqp(sq-poll)} or old version {io_uring-sq(sq-poll)}
sudo bpftrace --btf -e 'kretprobe:create_io_thread { @[retval] = count(); } interval:s:1 { print(@); clear(@); } END { clear(@); }' | cat -s
#sudo bpftrace -e 'tracepoint:io_uring:io_uring_submit_sqe {printf("%s(%d)\n", comm, pid);}'
```

2. if need use golang runtime native support, please see this Note: [#31908](https://github.com/golang/go/issues/31908)

3. 3rd io_uring support for golang:
    * https://github.com/hodgesds/iouring-go 
    * https://github.com/godzie44/go-uring 
    * https://github.com/Iceber/iouring-go
    * https://github.com/ii64/gouring [√] (one to one liburing cp,test coverage ok)

4. RocksDB MultiGet use IO Uring interface: https://github.com/facebook/rocksdb/wiki/MultiGet-Performance

## bench scene (net/storage IO)
1. net IO for netpoll scenes (more unbounded work stream requests, S_IFSOCK type fd)
    * tcp echo server, build & bench:
    ```shell
    make build-echo
    make bench-echo
    ```
2. storage IO for data file storage in HDD, NVMe SSD etc hardware scenes (more bounded work requests, eg: S_IFREG, S_ISBLK type fd)

## learn more try to change IO
* net IO
    1. redis: https://github.com/redis/redis/pull/9440
* storage IO
    1. badger: https://dgraph.io/blog/post/badger/
    2. pebble: https://www.cockroachlabs.com/blog/pebble-rocksdb-kv-store/

## linux kernel for new io_uring feature
### compiling linux kernel for develop io_uring-** tag branch
1. [kernel_compile](https://www.cyberciti.biz/tips/compiling-linux-kernel-26.html)
2. [the linux kernel archives](https://www.kernel.org/)
### upgrade release linux kernel for ubuntu 
https://sypalo.com/how-to-upgrade-ubuntu
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
* perf programe, cpu, memery, IO cost

Bench Tips:
* Bench net IO, use tcp tools: 
    * tcpdump check RST;
    * netstat check es,cw,tw stat and static tcp send recv;
    * ss static tcp stat;
    * <u>dstat</u> report processors related statistics iowait,sys use;,send,recv;
* Bench storage IO, use storage IO tools: 
    * fio bench ioengine, check IOPS, bw;
    * vmstat check io bi,bo, swap;
    * iostat check device r/w tps, iowait;
```shell
sudo apt install dstat
sudo apt install sysstat
sudo apt-get install linux-tools-common linux-tools-generic linux-tools-`uname -r`
```
[Perf](https://en.wikipedia.org/wiki/Perf_(Linux)) tools: 
* pprof(go): https://github.com/google/pprof
* gperftools(c/c++): https://github.com/gperftools/gperftools 
* [Linux kernel profiling with perf tutorial](https://perf.wiki.kernel.org/index.php/Tutorial)
* more perf tools : https://www.brendangregg.com/linuxperf.html

## bench result
test VM
> Distributor ID: Ubuntu <br>
Description: Ubuntu Lunar Lobster (development branch) <br>
Release: 23.04 <br>
Codename: lunar <br>
Linux ubuntu2 6.2.0-18-generic #18-Ubuntu SMP PREEMPT_DYNAMIC Thu Mar 16 00:09:48 UTC 2023 x86_64 x86_64 x86_64 GNU/Linux <br>
physical id 1 <br>
processor 4 <br>
cpu MHz : 1996.800 <br>
4  Intel(R) Core(TM) i5-1038NG7 CPU @ 2.00GHz <br>
MemTotal: 2005084 kB <br>

### **case1**. golang echo server (bound 1 core `taskset -cp 0 $SRV_PID`) bench

|                    | go-netpoll | go-iouring | go-iouring-sqpoll |
|--------------------|------------|------------|-------------------|
| c:300 bytes:128    | 38630 | 61884 | 78348 |
| c:500 bytes:128    | 44235 | 65096 | 76976 |
| c:1000 bytes:128   | 41551 | 62788 | 74774 |
| c:2000 bytes:128   | 40133 | 64316 | 78213 |
| c:300 bytes:512    | 42702 | 67029 | 82585 |
| c:500 bytes:512    | 40298 | 61839 | 73856 |
| c:1000 bytes:512   | 43908 | 65027 | 74533 |
| c:2000 bytes:512   | 41182 | 63614 | 71960 |
| c:300 bytes:1000   | 41143 | 65582 | 74441 |
| c:500 bytes:1000   | 39191 | 63407 | 77784 |
| c:1000 bytes:1000  | 42663 | 60978 | 74310 |
| c:2000 bytes:1000  | 42207 | 58816 | 66598 |

## reference
1. **https://unixism.net/loti/**
2. https://unixism.net/2020/04/io-uring-by-example-article-series/
3. windows IORing: https://windows-internals.com/ioring-vs-io_uring-a-comparison-of-windows-and-linux-implementations/ 
4. [Diego Didona - **<u>Understanding Modern Storage APIs: A systematic study of libaio, SPDK, and io_uring</u>**](https://atlarge-research.com/pdfs/2022-systor-apis.pdf) , [video](https://www.youtube.com/watch?v=5jKKVdJJqKY)
5. [awesome-iouring](https://github.com/espoal/awesome-iouring)
6. https://openanolis.cn/sig/high-perf-storage/doc/218455073889779745

