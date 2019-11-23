#ifndef _cache_h_
#define _cache_h_

#include "slab.h"
#include <mutex>

#define CACHE_NAMELEN 100
#define NUM_OF_MEM_BUFFS 13

typedef unsigned int kmem_bufctl_t;

#define slab_bufctl(slabPtr) \
	((kmem_bufctl_t*)(((slab_t*)slabPtr) + 1))

typedef struct slab_s{
	//struct slab_s* mySlabList; //lista
	unsigned int colourOff;
	void* s_mem;
	unsigned int inuse;
	kmem_bufctl_t free;
	struct slab_s* next;
}slab_t;

typedef struct kmem_cache_s{
	std::mutex *cacheMutex;
	std::mutex mutexLocation;

	slab_t* slabsFull;
	slab_t* slabsPartial;
	slab_t* slabsFree;
	unsigned objSize;
	unsigned objPerSlab;
	unsigned objInUse;

	unsigned numOfBlocks;
	unsigned numOfSlabs;
	unsigned colour;
	unsigned colourOff;
	unsigned colourNext;

	unsigned growing;

	void(*ctor)(void *);
	void(*dtor)(void *);

	char name[CACHE_NAMELEN];
	struct kmem_cache_s* next;
}kmem_cache_t;

typedef struct _cacheManager{
	kmem_cache_t* firstCache;
	unsigned	  numOfCaches;

	kmem_cache_t* sizesCache[NUM_OF_MEM_BUFFS];
}cacheManager;

extern cacheManager* cacheManagerPtr;

#endif