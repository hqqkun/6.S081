// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmemArray[NCPU];
void
kinit()
{
  for (int i = 0; i != NCPU; ++i) {
    initlock(&(kmemArray[i].lock), "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa) {
  struct run *r;
  struct kmem* kmemPtr;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  /* lab lock begin */
  push_off();
  int id = cpuid();
  pop_off();

  kmemPtr = &(kmemArray[id]);
  acquire(&(kmemPtr->lock));

  r->next = kmemPtr->freelist;
  kmemPtr->freelist = r;

  release(&(kmemPtr->lock));
  
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void) {
  struct run *r;
  struct kmem* kmemPtr;

  push_off();
  int id = cpuid();
  pop_off();
	int idEnd = id + NCPU;
	for (int i = id; i != idEnd; ++i) {

        kmemPtr = &(kmemArray[i % NCPU]);

	    acquire(&(kmemPtr->lock));
	    r = kmemPtr->freelist;
	    if (r) {
			kmemPtr->freelist = r->next;
			release(&(kmemPtr->lock));
			memset((char*)r, 5, PGSIZE);	// fill with junk
			return (void*)r;
		}
		release(&(kmemPtr->lock));
    }
	
	return (void*)0;
}
