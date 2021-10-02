# *Write in Chinese*

# Lab Page Tables



## 1	前言

探索页表并修改函数以简化从用户区域到内核区域的拷贝。

阅读 xv6 参考书的第三章，及相关文件：

​		kernel / memlayout.h

​		kernel / vm.c

​		kernel / kalloc.c

## 2	实验内容

### 2.1	Print a page table

规格化打印进程 1 的页表

```C
/* 在 kernel/vm.c 添加 */

/* xv6 采用三级页表，用 layer 控制递归条件 */
static void printPageTable(pagetable_t pagetable,int layer) {
    static int layer = 0;
	pte_t pte;
	uint64 child;
	/* 可以用循环代替 switch case */
    /* 512 是每个页表含有的 pte 个数 */
	for(int i = 0; i != 512; ++i){
		pte = pagetable[i];
		if(pte & PTE_V){
			switch(layer){
				case 2: printf(".. ");
				case 1: printf(".. ");
				case 0: printf(".. ");
			}
			printf("%d: pte %p pa %p\n",i,(void*)pte,(void*)PTE2PA(pte));
			
			if(layer == 2)
				continue;
			
			child = PTE2PA(pte);
            		++layer;
			printPageTable((pagetable_t)child);
            		--layer;
		}
	}
}
void vmprint(pagetable_t pagetable) {
	printf("page table %p\n",pagetable);
	printPageTable(pagetable);
}

/* 在 kernel/exec.c 的 exec() 中最后位置添加 */
int exec(char *path, char **argv) {
    ...
	if(p->pid == 1)
    	vmprint(p->pagetable);
    ...
}
```

不要忘记在 `kernel / defs.h` 添加 `vmprint()` 函数原型。



### 2.2	A kernel page table per process

在内核中执行程序时，内核页表将虚拟内存直接映射到物理内存，且 XV6 只有一个内核页表。然而 XV6 对于每个用户进程有独立页表，且只包含用户虚拟地址从 0 开始的映射。因此当内核需要用到用户通过系统调用传进去的指针时，需首先查其页表将其转化为物理地址。

因此，下面两个实验的目标就是允许内核直接提取用户指针。

****

要想使内核直接提取用户指针，且仍保持进程之间相互隔离，必须首先给每个进程分配***独立且一致***的内核页表，将进程与内核配套，创造一种内核只为该进程服务的假象。

所以在进程的 PCB 里加入内核的页表。

``` C
/* 在 kernel/proc.h, struct proc 内添加域 */
/* typedef uint64* pagetable_t （在 kernel/riscv.h 中定义）*/
	pagetable_t kernelPageTable;
```

#### 2.2.1	编写 `ukvmInit()`

不同于现代操作系统直接规定用户代码段必须从虚拟地址的某个地方开始（该地址以下指定为操作系统），XV6 用户进程直接从 `0` 地址开始执行。这就导致若内核页表不经修改，内核直接引用用户地址将造成无法预料的错误（0 ~ KERNBASE 为未使用及 I/O 设备映射，且尚未考虑映射到内核的情况）。

所以必须修改进程的内核页表，但<u>系统初始化时的内核页表不能修改</u>！

为此我们可以从 `0` 地址，将用户虚存地址覆盖用户内核虚存地址（这是本次实验最后一节的内容），且覆盖最大地址不能超过 `KERNBASE`（其上为内核指令与数据），同时实验手册又说明覆盖地址不能超过 `PLIC`。所以最大覆盖地址为 `PLIC`，即 `0xc000000`。

```c
/* 在 kernel/VM.c 中添加 */

/* 用户进程内核虚拟内存初始化 */
pagetable_t ukvmInit(void) {
	pagetable_t kernel = uvmcreate();
	if (kernel == 0)
		return (pagetable_t)0;
    /* 参照 kvminit() 函数，但并不映射 0 ~ PLIC，其地址范围包括 CLINT，所以不做 CLINT 的 mappages() */
    mappages(kernel, UART0, PGSIZE, UART0, PTE_R | PTE_W);
    mappages(kernel, VIRTIO0, PGSIZE, VIRTIO0, PTE_R | PTE_W);
    mappages(kernel, PLIC, 0x400000, PLIC, PTE_R | PTE_W);
    mappages(kernel, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R | PTE_X);
    mappages(kernel, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R | PTE_W);
    mappages(kernel, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X);
    return kernel;
}
```

kernel / proc.c 中的 `allocproc()` 创建进程 PCB，同时应初始化进程内核页表。而进程有独自的内核页表，`procinit()` 为每个进程提前建立内核栈的映射也就没必要了，可以放在 `allocproc()` 中执行。

```C
/* 在 kernel/proc.c 中添加 */

/* in allocproc() */
static struct proc* allocproc(void) {
    ...
	/* 建立内核映射 */
	p->kernelPageTable = ukvmInit();
	if (p->kernelPageTable == 0) {
        	freeproc(p);
       	 	release(&p->lock);
       		return 0;
    	}
	/* 建立进程内核栈映射 */
  	char *pa = kalloc();
  	if(!pa)
  		panic("allocproc: kalloc");
	/* 将所有的进程内核栈虚存地址统一为 Kstack0, 同时有 Guard Page */
  	uint64 va = KSTACK(0);
  	mappages(p->kernelPageTable, va, PGSIZE, (uint64)pa, PTE_R | PTE_W);
	p->kstack = va;
    ...
}
```

#### 2.2.2	修改 `scheduler()`

分析 `kernel/vm.c` 中 `kvminithart()` ，该函数是将页表寄存器初始化。首先将内核页表载入 satp 寄存器（w_satp），随后刷新（sfence_vma）。

```C
// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart() {
  	w_satp(MAKE_SATP(kernel_pagetable));
 	sfence_vma();
}
```

因此，在本实验中，若进程进入内核，则应当将 satp 寄存器赋予该进程的内核页表。根据指导书，修改 `scheduler()` 函数。同时，若没有进程在运转，则应运行系统内核初始的页表，即 `kernel_pagetable`。

```c
/* 在 kernel/proc.c 中添加 */

/* in scheduler() */
void scheduler(void) {
    ...
	for(p = proc; p < &proc[NPROC]; p++) {
      		acquire(&p->lock);
      		if(p->state == RUNNABLE) {
        		p->state = RUNNING;
        		c->proc = p;
          
       			w_satp(MAKE_SATP(p->kernelPageTable));
        		sfence_vma();
      
        		swtch(&c->context, &p->context);
       			kvminithart();
        
        		c->proc = 0;

        		found = 1;
      		}
      		release(&p->lock);
    	}		
    ...
}
```

#### 2.2.3	修改 `freeproc()`

进程退出的时候时候，需要释放 PCB，在 kernel/proc.c 中的 `freeproc()` 进行，所以在这添加释放进程页表的功能。

```c
/* 在 kernel/vm.c 中添加 */

/* free user's kernel page table */
/* 释放用户内核页表 */
/* 递归实现 */
void freeukvm(pagetable_t kernel) {
    static int layer = 0;
  	pte_t pte;
  	uint64 child;
  	if(layer == 2)
  		goto Done;
  	for (int i = 0; i != 512; ++i) {
  		pte = kernel[i];
		if (pte & PTE_V) {
			child = PTE2PA(pte);
            		++layer;
			freeukvm((pagetable_t)child);
           		--layer;
		}
	}
Done:
	kfree((void*)kernel);
}
```

  ```C
  /* 在 kernel/proc.c 中添加 */
  
  static void freeproc(struct proc *p) {
  	...
  	/* 
  	 * 进程退出时，释放内核栈 
  	 * do_free	:= 1, 释放内核栈页面
  	 */
  	if (p->kstack)
         	 uvmunmap(p->kernelPageTable, p->kstack, 1, 1);
      	if (p->kernelPageTable) {
      	/* 
         * 只需要释放内核页表即可，无需释放用户进程页面
         * 因为在前几行 proc_freepagetable(p->pagetable, p->sz); 已经释放了用户进程页面
         */
          	freeukvm(p->kernelPageTable); 
       }
       	p->kernelPageTable = 0;
       	p->kstack = 0;
  	 ...
  }
  ```

上面代码用到了 `uvmunmap()` ：

```C
/* 在 kernel/vm.c 中 */

/* 从 va 开始的 npages 个页面从 pagetable 所指定的页表中取消映射
 * do_free 若置位，则收回叶子物理页面
 */
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
```

####  2.2.4	修改 `kvmpa()`

 此时内核修改的差不多了，但若执行 `make qemu` 会发生 `panic: kvmpa` 错误。 查看 `kernel/vm.c` 中的 `kvmpa()` ，该函数将内核虚拟地址映射为物理地址，但仅仅为内核栈服务。所以应使用进程独立的内核页表，将 `kernel_pagetable` 替换为 `myproc()->kernelPageTable`。同时，因使用 `myproc()` ，需包含新的头文件。

  ```C
/* 在 kernel/vm.c */
#include "spinlock.h"
#include "proc.h"

/* 根据注释可以看到，这是关于进程内核栈的映射，所以将源代码改写为使用每个进程的内核页表 */
uint64
kvmpa(uint64 va) {
    ...
    pte = walk(myproc()->kernelPageTable, va, 0);
 	...
}
  ```



### 2.3	Simplify copyin/copyinstr

```C
/* 在 kernel/vm.c */
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) 
```

未修改的 xv6 内核在执行 `copyin()` 时，收到的参数是用户进程的 `srcva` ，然后通过访问用户页表得到实际的物理地址，比较复杂，所以现在要做的就是在进程进入内核态时，仅访问内核页表而无需访问用户页表得到 `srcva` 对应的实际物理地址。

很自然的想法就是把用户进程的页表添加到内核中，用户进程的代码和数据是从其虚址 `0` 开始，所以可以将内核页表从虚址 `0` 开始映射到用户进程（这样每个用户进程的物理页面被映射了两次，一次在用户页表，一次在内核页表）。

#### 2.3.1	编写 `mapUvm2Ukvm()`

`mapUvm2Ukvm()` 即为将用户进程页表添加到该进程的内核页表。

注意，标志为 `PTE_U` 的页面不能在内核中使用，所以映射时，取消该位。当前可以不用关注 `if` 语句内部函数。

```C
/* 在 kernel/vm.c 中添加 */

/* start 	:= 	需要映射的起始虚址
 * sz 		:=	用户进程的大小 
 */
void mapUvm2Ukvm(pagetable_t pagetable, pagetable_t kernelPageTable, uint64 start, uint64 sz) {
    	pte_t* user, *kernel;
   	if (start >= sz) {
        /* this is for sbrk() */
        	uint64 npages = (PGROUNDUP(start) - PGROUNDUP(sz)) / PGSIZE;
       	 	uvmunmap(kernelPageTable, PGROUNDUP(sz), npages, 0);
       	 	return;
    	}
    	else {
        	sz = PGROUNDUP(sz);
        	for (start = PGROUNDUP(start); start != sz; start += PGSIZE) {
           		user = walk(pagetable, start, 0);
            		kernel = walk(kernelPageTable, start, 1);
            		*kernel = (*user) & ~PTE_U;
        	}
    	}
}
```

#### 2.3.2	修改 `userinit()`

`userinit()` 初始化第一个进程，并执行映射。

```C
/* 在 kernel/proc.c 中 */

/* in userinit() */
void userinit(void) {
	...
	p->state = RUNNABLE;
    mapUvm2Ukvm(p->pagetable, p->kernelPageTable, 0, p->sz);
 	release(&p->lock);
}
```

#### 2.3.3	修改 `fork()`

`fork()` 创建子进程，子进程同样要执行用户进程与内核的映射。

```C
/* 在 kernrl/proc.c 中 */

int fork(void) {
    ...
    np->state = RUNNABLE;
    /* np 即为子进程 */
    mapUvm2Ukvm(np->pagetable, np->kernelPageTable, 0, np->sz);
  	release(&np->lock);
  	return pid;
}
```

#### 2.3.4	修改 `exec()`

`exec()` 比 `fork()` 要复杂不少， 首先用户进程的大小可以超过 `CLINT`，但不能超过 `PLIC` ，所以应对其进行检查。

```C
/* 在 kernel/exec.c 中 */

int exec(char *path, char **argv){
    ...
    for (i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)) {
        ...
        uint64 sz1;
    	if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      		goto bad;
    	if (sz1 > PLIC)
      		goto bad;
        ...
    }
    ...
}
```

`exec()` 的作用是执行 `path` 路径下的文件， 所以应该取消内核原用户进程的映射，随后执行 `path` 文件的映射。

```C
/* 在 kernel/exec.c 中 */

int exec(char *path, char **argv){
    ...
    proc_freepagetable(oldpagetable, oldsz);
    
    /* 取消原进程的内存映射, 不能 do_free, 否则会把父进程的页面收回 */
    uvmunmap(p->kernelPageTable, 0, PGROUNDUP(oldsz) / PGSIZE, 0);
    mapUvm2Ukvm(p->pagetable, p->kernelPageTable, 0, p->sz);
    
    if (p->pid == 1) 
        vmprint(p->pagetable);
    return argc;
}
```

需要注意的是，执行取消映射的 `uvmunmap()` 时，不可以 `do_free` ，否则会将父进程的页面收回。因为用户通常会编写如下代码：

```C
if (!fork()) {
 	exec()
}
```

#### 2.3.5	修改 `growproc()`

`growproc()` 是系统调用 `sbrk()` 的处理函数，相应用户进程动态的增加、减少内存。

```C
/* 在 kernel/proc.c 中 */

int growproc(int n) {
   	uint sz;
    /* oldsz := 进程原占用空间 */
  	uint oldsz;
  	struct proc *p = myproc();

  	oldsz = sz = p->sz;
    ...
    p->sz = sz;
 	mapUvm2Ukvm(p->pagetable, p->kernelPageTable, oldsz, p->sz);
 	return 0;
}
```

现在，重新审视 `mapUvm2Ukvm()` 。若此时进程内存为减少，则 `start >= sz` ，执行 `if` 。

 ```C
 /* 在 kernel/vm.c 中添加 */
 
 /* start 	:= 	需要映射的起始虚址
  * sz 		:=	用户进程的大小 
  */
 void mapUvm2Ukvm(pagetable_t pagetable, pagetable_t kernelPageTable, uint64 start, uint64 sz) {
     pte_t* user, *kernel;
     if (start >= sz) {
         /* this is for sbrk() */
         /* npages 即为要释放的页面数，用 PGROUNDUP() 对齐 */
         uint64 npages = (PGROUNDUP(start) - PGROUNDUP(sz)) / PGSIZE;
         
         /* 
          * do_free := 0, 因为在 growproc() 内的 uvmdealloc() 函数内部已经释放了具体的页面
          * 现在只是取消映射
          */
         
         uvmunmap(kernelPageTable, PGROUNDUP(sz), npages, 0);
         return;
     }
     else {
         sz = PGROUNDUP(sz);
         for (start = PGROUNDUP(start); start != sz; start += PGSIZE) {
             user = walk(pagetable, start, 0);
             kernel = walk(kernelPageTable, start, 1);
             *kernel = (*user) & ~PTE_U;
         }
     }
 }
 ```

#### 2.3.6	修改 `copyin()` 与 `copyinstr()`

```C
/* 在 kernel/vm.c 中 */

int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
    return copyin_new(pagetable, dst, srcva, len);
}

int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max) {
    return copyinstr_new(pagetable, dst, srcva, max);
}
```





最后，不要忘记将所编写的各种函数在 `kernel/defs.h` 中加以声明。
