# Lab Lazy Allocation

## 1	前言

在出现缺页中断时，为进程分配页面。

阅读 xv6 参考书的第四章，及相关文件：

kernel / trap.c

kernel / vm.c

kernel / sysproc.c

## 2	实验内容

### 2.1	Eliminate allocation from `sbrk()`

传统的 `sbrk()` 会直接满足进程的需求，为其分配足够的内存，但很多时候，进程申请的内存远大于其实际所需。

所以，在 `lazy` 的处理方式中，`sbrk()` 只是象征性地改变其进程大小，而不实际分配物理地址，在出现缺页中断时再处理。

```C
/* 在 kernel/sysproc.c 中 */

uint64 sys_sbrk(void) {
	int addr;
 	int n;
  	struct proc* p = myproc();

  	if(argint(0, &n) < 0)
    	return -1;
  	addr = p->sz;
  	p->sz += n;

  	if (n < 0) 
    	uvmdealloc(p->pagetable, addr, p->sz);
  	return addr;
}
```



### 2.2	Lazy allocation

修改完 `sys_sbrk()` 之后运行进程会出现缺页错误，而 `kernel / trap.c` 处理各种异常。

```C
/* 在 kernel/trap.c 中修改 */

void usertrap(void) {
    ...
    uint64 cause = r_scause();
    ...
    else if ((which_dev = devintr()) != 0) {
    // ok
  	}
    
    /* cause := 13, 为 Load  异常
     * cause := 15, 为 Store 异常 
     */
  	else if (cause == 13 || cause == 15) {
        /* addr := 产生缺页异常的地址 */
    	uint64 addr = r_stval();
        
    	char* mem = (char*)kalloc();
    	if (mem == 0) {
        	p->killed = 1;
    	}
    	else {
        	memset(mem, 0, PGSIZE);
            /* addr 向下取整，得到虚拟地址页面号码 */
            addr = PGROUNDDOWN(addr);
        	if (mappages(p->pagetable, addr, PGSIZE, (uint64)mem, PTE_R | PTE_W | PTE_U) != 0) {
            	kfree((void*)mem);
            	p->killed = 1;
        	}
    	}
  	}
    ...
  	if(p->killed)
    	exit(-1);
}
```

这样修改之后，内核就会在产生缺页中断时，为进程自动分配页面了，当然，为了能让 `echo()` 运行成功，省略了各种检查条件，将在 `2.3` 节中进行重写。

此时运行 `echo()` ，虽然不再出现中断异常，但却出现了 `panic: uvmunmap: not mapped` 错误，来自于 `kernel / vm.c` 文件。在进程退出时，会调用 `uvmunmap()` ，传统的 `xv6` 保证用户进程全部页面都被页表映射，而 `lazy` 方法则不然，会出现页面没有被映射的情况，所以应删除相关 `panic()` 语句（这种情况不是 panic）。

```C
/* 在 kernel/vm.c 中修改 */
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
  	uint64 a;
  	pte_t *pte;

  	if ((va % PGSIZE) != 0)
    	panic("uvmunmap: not aligned");

   for (a = va; a < va + npages*PGSIZE; a += PGSIZE) {
       if ((pte = walk(pagetable, a, 0)) == 0)
      		continue;
       if ((*pte & PTE_V) == 0)
      		continue;
       if (PTE_FLAGS(*pte) == PTE_V)
           panic("uvmunmap: not a leaf");
       if (do_free) {
           uint64 pa = PTE2PA(*pte);
           kfree((void*)pa);
       }
       *pte = 0;
   }
}
```



### 2.3	Lazytests and Usertests

以下所做为增强内核的健壮性，防止恶意进程破坏内核运行。

#### 2.3.1	处理 `sbrk()` 负数参数

在 `sys_sbrk()` 中已处理完毕。  

#### 2.3.2	处理访问越界

若出现缺页异常的地址已经超过了进程申请的地址，则应直接杀死进程。

为避免 `usertrap()` 过于庞杂，对其进行重构。

```C
/* 在 kernel / trap.c 中添加 */

void usertrap(void) {
    ...
  	else if (cause == 13 || cause == 15) {
        if (pageFaultHandler(p) < 0)
            p->killed = 1;
  	}
    ...
}

static int pageFaultHandler(struct proc* p) {
    uint64 addr = r_stval();
    char* mem;
    // 若 addr 地址超过合理范围，则返回 -1, 代表出错
    if (addr >= p->sz)
        return -1;

    if ((mem = (char*)kalloc()) == 0) {
        return -1;
    }
    memset(mem, 0, PGSIZE);
    addr = PGROUNDDOWN(addr);
    if (mappages(p->pagetable, addr, PGSIZE, (uint64)mem, PTE_R | PTE_W | PTE_U) != 0) {
        kfree((void*)mem);
        return -1;
    }
    return 0;
}
```

#### 2.3.3	处理访问栈以下地址

在内核中运行时，`trapframe` 保存进程的用户态镜像，根据 xv6 的指导书的  `3.6` 节的用户进程空间，唯一访问栈以下地址且出现异常的行为就是访问了 `guard page` ，此时直接杀死进程（简单粗暴）。

```C
/* 在 kernel / trap.c 中添加 */

static int pageFaultHandler(struct proc* p) {
    ...
    if (addr >= p->sz || addr <= p->trapframe->sp)
        return -1;
    ...
}
```

#### 2.3.4	处理内存溢出

在 `pageFaultHandler()` 中已处理完毕。

#### 2.3.5	处理父子进程在 `fork()` 时拷贝问题

运行 xv6，执行 `lazytests` 会出现 `panic: uvmcopy: page not present` 错误。

该错误因为父进程在执行 `fork()` 时，会调用 `uvmcopy()` ，因为传统的 `xv6` 保证进程所申请的空间一定有内存映射，而 `lazy` 方法则不要求，所以此时应删除相应的 `panic()` 语句。

```C
/* 在 kernel/vm.c 中修改 */

int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
    ...
    for (i = 0; i < sz; i += PGSIZE) {
        if((pte = walk(old, i, 0)) == 0)
            continue;
        if((*pte & PTE_V) == 0)
            continue;
        ...
    }
  }
	...
}
```

#### 2.3.5	处理系统调用所引发异常

在 `lazy` 分配中，当进程使用 `read()` 或 `write()` 系统调用时，用户进程缓冲区的地址可能尚未被分配。但此时因为系统运行在内核态，所以不会出现缺页中断（`lazy` 仅针对用户进程）。

`read()` 或 `write()` 最终会调用位于 `kernel / vm.c` 中的 `copyin()` 、`copyinstr()` 及 `copyout()` 。所以对这三个函数进行修改。

具体分析 `copyin()` ：

```C
/* 在 kernel/vm.c 中 */

int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
    ...
    while (len > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if(pa0 == 0)
            return -1;
      	...
    }
  return 0;
}
```

在 `lazy` 方法中，`pa0` 很容易得到 `0` 从而返回 `-1` 表示出错。但此时应该为检查是否可以为用户进程分配一页（`srcva` 所在页面）。在处理拷贝问题时，内核通过 `walk()` 用户页表的方式，所以不会运行 `kernel / trap` 中的函数。

因为分配页面的代码与 `pageFaultHandler()` 里相似，所以将其解耦重构，使得 `copyin()` 可以用到其内部大量代码。

~~~C
/* 在 kernrl/vm.c 中添加 */

#include "spinlock.h"
#include "proc.h"

/* 将 pageFaultHandler() 代码放入如下函数内 */
/* va := 发生缺页异常的虚址 */
int checkAndMapFaultPage(struct proc* p, uint64 va, uint64* paPtr) {
	char* mem;
	if (va >= p->sz || va <= p->trapframe->sp)
		return -1;
	if ((mem = (char*)kalloc()) == 0) 
		return -1;
	memset(mem, 0, PGSIZE);
	va = PGROUNDDOWN(va);
	if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_R | PTE_W | PTE_U) != 0) {
		kfree((void*)mem);
		return -1;
	}
	if (paPtr)
		*paPtr = (uint64)mem;
	return 0;
}
~~~

修改 `copyin()` 、`copyinstr()` 及 `copyout()` 。以 `copyin()` 为例：

```C
/* 在 kernel/vm.c 中 */

int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
    ...
    while (len > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0) {
            // 注意这里
            // 为避免再一次使用 walkaddr() 获取 pa0, 这里直接将 &pa0 传入函数。
            if (checkAndMapFaultPage(myproc(), va0, &pa0) < 0)
                return -1;
        }
      	...
    }
  return 0;
}
```

最后修改最初的 `pageFaultHandler()` ：

```C
/* 在 kernel/trap.c 中修改 */

static int pageFaultHandler(struct proc* p) {
    uint64 addr = r_stval();
    // 无需传递指针，所以传入 0
    return checkAndMapFaultPage(p, addr, 0);
}
```

不要忘记在 `kernel / defs.h` 中添加函数声明。