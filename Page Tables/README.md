# Lab Page Tables



## 1	前言

探索页表并修改函数以简化从用户区域到内核区域的拷贝。

阅读 xv6 参考书的第三章，及相关文件：

​		kernel / memlayout.h

​		kernel / vm.c

​		kernel / kalloc.c

## 2	实验内容

### 2.1	Print a page table

​		规格化打印进程 1 的页表

```C
/* 在 kernel/vm.c 添加 */

/* xv6 采用三级页表，用 layer 控制递归条件 */
static void 
print_page_table(pagetable_t pagetable,int layer)
{
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
			printf("%d: pte %p pa %p\n",i,(void*)pte,(void*)PTE2PA(pte) );
			
			if(layer == 2)
				continue;
			
			child = PTE2PA(pte);
			print_page_table((pagetable_t)child,layer + 1);
		}
	}
}
void vmprint(pagetable_t pagetable)
{
	printf("page table %p\n",pagetable);
	print_page_table(pagetable,0);
}

/* 在 kernel/exec.c 的 exec() 中合适位置添加 */
if(p->pid == 1)
    vmprint(p->pagetable);
```

不要忘记在 kernel / defs.h 添加 vmprint() 函数原型。



### 2.2	A kernel page table per process

​		在内核中执行程序时，内核页表将虚拟内存直接映射到物理内存，且 XV6 只有一个内核页表。然而 XV6 对于每个用户进程有独立页表，且只包含用户虚拟地址从 0 开始的映射。因此当内核需要用到用户通过系统调用传进去的指针时，需首先查其页表将其转化为物理地址。

​		因此，下面两个实验的目标就是允许内核直接提取用户指针。

****

​		要想使内核直接提取用户指针，且仍保持进程之间相互隔离，必须首先给每个进程分配***独立且一致***的内核页表，将进程与内核配套，创造一种内核只为该进程服务的假象。

​		所以在进程的 PCB 里加入内核的页表。

``` C
/* 在 kernel/proc.h, struct proc 内添加域 */
	/* typedef uint64* pagetable_t （在 kernel/riscv.h 中定义）*/
	pagetable_t kernel_page_table;
```

​		不同于现代操作系统直接规定用户代码段必须从虚拟地址的某个地方开始（该地址以下指定为操作系统），XV6 用户直接从 0 地址开始。这就导致若内核页表不经修改，内核直接引用用户地址将造成无法预料的错误（0 ~ KERNBASE 为未使用及 I/O 设备映射，且尚未考虑映射到内核的情况）。

​		所以必须修改进程的内核页表，但<u>系统初始化时的内核页表不能修改</u>！

​		为此我们可以从 0 地址，将用户虚存地址覆盖用户内核虚存地址（这是实验最后一节的内容），但覆盖最大地址不能超过 KERNBASE（其上为内核指令与数据），同时实验手册又说明覆盖地址不能超过 PLIC。所以最大覆盖地址为 PLIC，即 0xc000000。

```c
/* 在 kernel/proc.c 中添加 */
extern char etext[];
/* user process's kernel virtual memory initialization */
/* 用户进程内核虚拟内存初始化 */
static 
pagetable_t
ukvminit(void) 
{
	pagetable_t kernel = uvmcreate();
	if(!kernel)
		return (pagetable_t)0;
    /* 参照 kvminit() 函数，但并不映射 0 ~ PLIC，其地址范围包括 CLINT，所以不做 CLINT 的 mappages() */
    /* etext 全局变量，可转化为内核代码段的长度，需 extern 引用 */
  	mappages(kernel, UART0, PGSIZE, UART0, PTE_R | PTE_W);
  	mappages(kernel, VIRTIO0, PGSIZE, VIRTIO0, PTE_R | PTE_W);
  	mappages(kernel, PLIC, 0x400000, PLIC, PTE_R | PTE_W);
  	mappages(kernel, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R | PTE_X);
  	mappages(kernel, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R | PTE_W);
  	mappages(kernel, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X);
  	return kernel;
}
```

​		kernel / proc.c 中的 allocproc() 创建进程 PCB，同时应初始化进程内核页表。而进程有独自的内核页表，procinit() 为每个进程提前建立内核栈的映射也就没必要了，可以放在 allocproc() 中执行。

```C
/* 在 kernel/proc.c 中添加 */
/* in allocproc() */
	/* 建立内核映射 */
	p->kernel_page_table = ukvminit();
	/* 建立进程内核栈映射 */
  	char *pa = kalloc();
  	if(!pa)
  		panic("kalloc");
	/* 将所有的进程内核栈虚存地址统一为 Kstack0, 同时有 Guard Page */
  	uint64 va = TRAMPOLINE - 2 * PGSIZE;
  	mappages(p->kernel_page_table, va, PGSIZE, (uint64)pa, PTE_R | PTE_W);
	p->kstack = va;
```



### to be continued.

