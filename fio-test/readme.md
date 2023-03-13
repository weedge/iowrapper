### fio配置说明

```shell
fileame=/dev/sdb1   #测试文件名称，通常选择需要测试的盘的data目录
direct=1             #测试过程绕过机器自带的buffer。使测试结果更真实
rw=randwrite         #测试随机写的I/O
rw=randrw            #测试随机写和读的I/O
bs=16k               #单次io的块文件大小为16k
bsrange=512-2048     #同上，提定数据块的大小范围
size=5G              #本次的测试文件大小为5g，以每次4k的io进行测试
numjobs=30           #本次的测试线程为30个
runtime=1000         #测试时间1000秒，如果不写则一直将5g文件分4k每次写完为止
ioengine=libaio      #io引擎使用libaio方式, fio --enghelp 查看支持的io引擎
rwmixwrite=30        #在混合读写的模式下，写占30%
group_reporting      #关于显示结果的，汇总每个进程的信息

lockmem=1G           #只使用1g内存进行测试
zero_buffers         #用0初始化系统buffer
nrfiles=8            #每个进程生成文件的数量
log_avg_msec=1000    #日志打印时间
```

fio的读写模式
```shell
rw=read 顺序读
rw=write 顺序写
rw=readwrite 顺序混合读写
rw=randwrite 随机写
rw=randread 随机读
rw=randrw 随机混合读写
```

[fio-iouring.conf](./fio-iouring.conf)

主要关注bw和iops结果:

1. bw：磁盘的吞吐量，这个是顺序读写考察的重点

2. iops：磁盘的每秒读写次数，这个是随机读写考察的重点


---

### 硬盘性能指标
1. 顺序读写 （吞吐量，常用单位为MB/s）：文件在硬盘上存储位置是连续的。
适用场景：大文件拷贝（比如视频音乐）。速度即使很高，对数据库性能也没有参考价值。

2. 4K随机读写 （IOPS，常用单位为次）：在硬盘上随机位置读写数据，每次4KB。
适用场景：操作系统运行、软件运行、数据库。


SSD是固态硬盘、HDD是机械硬盘、HHD是混合硬盘。SATA和SAS分别是链接电脑主板的接口类型。根据个人需求的不同，这三种硬盘类型各有优点。如果你追求读写速度最好的就是SSD硬盘，如果你想要价格便宜，HDD则是最佳选择，HHD硬盘则性价比相对高一些，读写速度要比HDD快一些但价格相差并不太多。一次磁盘读 io 2ms 级别，SSD则在几十us

附：Latency Numbers Every Programmer Should Know： https://colin-scott.github.io/personal_website/research/interactive_latency.html

---

### 测试
这里测试不区分硬盘类型，主要测试aio和io_uring的读写对比

> 块大小：4kb <br> 
队列深度：128 <br> 
oengine: libaio/io_uring <br>
io_uring引擎下开启sqthread_pool <br>

顺序IO - 实验libaio和io_uring对比分4组:
1. libaio vs io_uring(sqthread_pool) 顺序读
2. libaio vs io_uring(sqthread_pool) 顺序写
3. libaio vs io_uring(sqthread_pool) 顺序读写 70%读
4. libaio vs io_uring(sqthread_pool) 顺序读写 70%写

随机IO - 实验libaio和io_uring对比分4组:
1. libaio vs io_uring(sqthread_pool) 随机读
2. libaio vs io_uring(sqthread_pool) 随机写
3. libaio vs io_uring(sqthread_pool) 随机读写 70%读
4. libaio vs io_uring(sqthread_pool) 随机读写 70%写

```
(io_uring)sqthread_poll
    Normally fio will submit IO by issuing a system call to notify the kernel of available items  in the SQ ring. If this option is set, the act of submitting IO will be done by a polling thread in the kernel. This frees up cycles for fio, at the cost of using more CPU in the system.
```

配置文件： 
1. [fio-libaio-iouring.seq.conf](./fio-libaio-iouring.seq.conf)
2. [fio-libaio-iouring.rand.conf](./fio-libaio-iouring.rand.conf)

---

### 画图
fio安装完后自带有一个高级脚本fio_generate_plots能够根据fio输出的数据进行画图。

fio的输出日志主要包含三种：bw，lat和iops，设置这三种的参数如下：

```
write_bw_log=rw
write_lat_log=rw
write_iops_log=rw
```
后面接的参数rw，是输出日志文件名的prefix，如最终会生成的日志文件如下
```
rw_iops.{i}.log
rw_clat.{i}.log
rw_slat.{i}.log
rw_lat.{i}.log
rw_bw.{i}.log
```
使用下面的命令即可自动画图(依赖gnuplot)：
```shell
#sudo apt-get install gnuplot
fio_generate_plots libaio-iouring.{seq/rand}
```

### reference
2. https://github.com/dgraph-io/badger-bench/blob/master/BENCH-rocks.txt
3. https://github.com/dgraph-io/badger-bench/blob/master/BENCH-lmdb-bolt.md


