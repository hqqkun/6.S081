# 6.s081	FALL 	2020

MIT，操作系统

## 课程平台

[6.S081 / Fall 2020 (mit.edu)](https://pdos.csail.mit.edu/6.828/2020/index.html)

## 实验进度

- [x] [Utilities](https://github.com/hqqkun/6.S081/tree/main/Utilities)
- [x] [System Calls](https://github.com/hqqkun/6.S081/tree/main/System%20Calls)
- [x] [Page Tables](https://github.com/hqqkun/6.S081/tree/main/Page%20Tables)
- [x] [Traps](https://github.com/hqqkun/6.S081/tree/main/Traps)
- [x] [Lazy Allocation](https://github.com/hqqkun/6.S081/tree/main/Lazy%20allocation)
- [x] [Copy On Write](https://github.com/hqqkun/6.S081/tree/main/Copy-on-Write%20Fork)
- [x] [Multithreading](https://github.com/hqqkun/6.S081/tree/main/Multithread)
- [x] [Clock](https://github.com/hqqkun/6.S081/tree/main/Lock)
- [x] File System
- [x] Mmap

## 实验环境搭建

### 平台

`ubuntu-20.04`

[Ubuntu 官网最新版](https://ubuntu.com/download/desktop)

[Ubuntu 镜像下载](https://launchpad.net/ubuntu/+cdmirrors)

### 安装实验环境

```shell
$ sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

### 下载 `xv6` 并编译

```shell
$ git clone git://g.csail.mit.edu/xv6-labs-2020
$ cd xv6-labs-2020
$ git checkout util
$ make qemu
```

