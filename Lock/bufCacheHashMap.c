#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define PRIME 31
#define BUFPERBUCKET 5

struct cacheBucket {
    struct spinlock lock;
    struct buf  bufArray[BUFPERBUCKET];
};

struct cacheBucket cacheHashTable[PRIME];

/*  static function prototype */
static struct buf* LRUevict(struct buf bufArray[], int length);
 

void bufCacheInit(void) {
    struct cacheBucket* bucketPtr;
    struct buf* bufPtr;

    for (int i = 0; i != PRIME; ++i) {
        bucketPtr = &(cacheHashTable[i]);
        initlock(&(bucketPtr->lock), "bcache");

        bufPtr = bucketPtr->bufArray;
        for (int j = 0; j != BUFPERBUCKET; ++j) {
            initsleeplock(&(bufPtr[j].lock), "bcache");
        }
    }
}

/* must hold cacheBucket's spinlock before calling this routine */
static struct buf* LRUevict(struct buf bufArray[], int length) {
    struct buf* bufPtr = 0;
    uint time = -1;

    for (int i = 0; i != length; ++i) {
        if (bufArray[i].refcnt) {
            continue;
        }
        if (bufArray[i].time < time)
            bufPtr = &bufArray[i], time = bufArray[i].time;        
    }
    return bufPtr;
}

struct buf* findEntry(uint dev, uint blockno) {
    uint bucketNo = blockno % PRIME;
    struct cacheBucket* bucketPtr = &(cacheHashTable[bucketNo]);
    struct buf* bufPtr;

    acquire(&bucketPtr->lock);

    for (int i = 0; i != BUFPERBUCKET; ++i) {
        bufPtr = &(bucketPtr->bufArray[i]);
        if (bufPtr->dev == dev && bufPtr->blockno == blockno) {
            ++(bufPtr->refcnt);
            release(&bucketPtr->lock);
            acquiresleep(&bufPtr->lock);
            return bufPtr;
        }
    }

    // sad, we don't cache it, so do the LRU
    bufPtr = LRUevict(bucketPtr->bufArray, BUFPERBUCKET);
    if (!bufPtr)
        panic("bufCacheHashMap->find: no buffers");
   
    bufPtr->refcnt = 1;
    bufPtr->dev = dev;
    bufPtr->blockno = blockno;
    bufPtr->valid = 0;

    acquiresleep(&bufPtr->lock);
    release(&bucketPtr->lock);
    return bufPtr;
}

/* bufPtr->ref == 0 only if relese will be done */
void relseEntry(struct buf* bufPtr) {
    uint blockno = bufPtr->blockno;
    uint bucketNo = blockno % PRIME;

    struct cacheBucket* bucketPtr = &(cacheHashTable[bucketNo]);

    // we have already hold bufPtr->lock, so just hold the cacheBucket's lock
    acquire(&bucketPtr->lock);

    --(bufPtr->refcnt);
    if (!bufPtr->refcnt) {
        bufPtr->time = ticks;
    }
    release(&bucketPtr->lock);
    releasesleep(&bufPtr->lock);
}