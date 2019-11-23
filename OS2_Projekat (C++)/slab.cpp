#include "slab.h"
#include "buddy.h"
#include "cache.h"
#include <string.h>
#include <cmath>
#include <stdio.h>

cacheManager* cacheManagerPtr;
std::mutex mtx;

void kmem_init(void *space, int block_num){
	//mtx.lock();

	buddyInit(space, block_num);
	cacheManagerPtr = (cacheManager*)buddyGet(1);
	cacheManagerPtr->firstCache = 0;
	cacheManagerPtr->numOfCaches = 0;

	int i;
	for (i = 0; i < NUM_OF_MEM_BUFFS; i++){
		char name[30];
		sprintf(name, "size-2^%d", i + 5);
		size_t size = pow(2.0, i + 5);
		cacheManagerPtr->sizesCache[i] = _kmem_cache_create(name, size, 0, 0);
	}

	//mtx.unlock();
}

kmem_cache_t *kmem_cache_create(const char *name, size_t size, void(*ctor)(void *), void(*dtor)(void *)){
	mtx.lock();
	kmem_cache_t* ret = _kmem_cache_create(name, size, ctor, dtor);
	mtx.unlock();

	return ret;
}

kmem_cache_t *_kmem_cache_create(const char *name, size_t size, void(*ctor)(void *), void(*dtor)(void *)){
	kmem_cache_t* cache = (kmem_cache_t*)buddyGet(1);

	cache->cacheMutex = new((void*)(&cache->mutexLocation))std::mutex();

	cache->slabsFree = 0;
	cache->slabsFull = 0;
	cache->slabsPartial = 0;

	cache->numOfSlabs = 0;
	cache->objSize = size;
	cache->objInUse = 0;
	int numOfBlocks = pow(2.0, ceil(log(ceil((double)size / BLOCK_SIZE)) / log(2.0))); //closest upper power of 2
	unsigned int totalMem = 0;

	totalMem += sizeof(slab_t);
	int i = 0;
	while (1){
		if ((totalMem + size + sizeof(unsigned int)) < (numOfBlocks*BLOCK_SIZE))
			totalMem += size + sizeof(unsigned int);
		else break;
		i++;
	}

	cache->objPerSlab = i;

	if (i == 0){
		cache->objPerSlab = 1;
		numOfBlocks*=2;
	}

	cache->numOfBlocks = numOfBlocks;

	int wastage = numOfBlocks*BLOCK_SIZE - totalMem;
	cache->colour = floor(wastage / CACHE_L1_LINE_SIZE);
	cache->colourOff = CACHE_L1_LINE_SIZE;
	cache->colourNext = 0; //next colour line to use; wraps back to 0 when reach colour

	cache->growing = 0;
	cache->ctor = ctor;
	cache->dtor = dtor;
	strcpy(cache->name, name);

	
	if (name[4]!='-'){
		cache->next = cacheManagerPtr->firstCache;
		cacheManagerPtr->firstCache = cache;
		cacheManagerPtr->numOfCaches++;
	}

	return cache;
}

void *kmem_cache_alloc(kmem_cache_t *cachep){
	cachep->cacheMutex->lock();
	//mtx.lock();
	void* obj = _kmem_cache_alloc(cachep);

	cachep->cacheMutex->unlock();
	//mtx.unlock();

	return obj;	
}

void* _kmem_cache_alloc(kmem_cache_t *cachep){
	void* object = 0;
	
	int size = cachep->objSize;

	if (cachep->slabsPartial){
		slab_t* slab = cachep->slabsPartial;
		object = (unsigned char*)slab->s_mem + slab->free*cachep->objSize;
		slab->free = slab_bufctl(slab)[slab->free];
		slab->inuse++;

		if (slab->inuse == cachep->objPerSlab){
			cachep->slabsPartial = slab->next;
			slab->next = cachep->slabsFull;
			cachep->slabsFull = slab;
		}
	}
	else {
		if (!cachep->slabsFree){
			kmem_cache_grow(cachep);
		}
		slab_t* slab = cachep->slabsFree;
		object = (unsigned char*)slab->s_mem + slab->free*cachep->objSize;
		slab->free = slab_bufctl(slab)[slab->free];
		slab->inuse++;

		cachep->slabsFree = slab->next;
		if (slab->inuse < cachep->objPerSlab){
			slab->next = cachep->slabsPartial;
			cachep->slabsPartial = slab;
		}
		else{
			slab->next = cachep->slabsFull;
			cachep->slabsFull = slab;
		}
	}

	cachep->objInUse++;
	return object;
}

void kmem_cache_grow(kmem_cache_t *cachep){
	int si = cachep->objSize; //DEBUG

	slab_t* newSlab = (slab_t*)buddyGet(cachep->numOfBlocks);
	cachep->numOfSlabs++;
	newSlab->colourOff = cachep->colourNext*CACHE_L1_LINE_SIZE;
	if (cachep->colourNext++ == cachep->colour) cachep->colourNext = 0;

	//bufctl init
	int i;
	newSlab->free = 0;
	for (i = 0; i < cachep->objPerSlab; i++){
		if (i == cachep->objPerSlab - 1)
			slab_bufctl(newSlab)[i] = UINT_MAX; //no more free objects
		else
			slab_bufctl(newSlab)[i] = i + 1;
	}

	newSlab->s_mem = ((unsigned char*)(((unsigned int*)(newSlab + 1)) + cachep->objPerSlab - 1)) + newSlab->colourOff;

	//objects init
	for (i = 0; i < cachep->objPerSlab; i++){
		void *object = (unsigned char*)newSlab->s_mem + cachep->objSize*i;
		if (cachep->ctor)
			(*(cachep->ctor))(object);
	}

	cachep->growing = 1;

	newSlab->inuse = 0;
	newSlab->next = cachep->slabsFree;
	cachep->slabsFree = newSlab;
}

int kmem_cache_shrink(kmem_cache_t *cachep){
	cachep->cacheMutex->lock();
	//mtx.lock();
	int ret = _kmem_cache_shrink(cachep);
	
	cachep->cacheMutex->unlock();
	//mtx.unlock();

	return ret;
}

int _kmem_cache_shrink(kmem_cache_t *cachep){
	if (cachep->growing == 1){
		cachep->growing = 0;
		return 0;
	}

	int freedBlocks = 0;
	slab_t* freeSlab = cachep->slabsFree;
	while (freeSlab){
		int i;
		for (i = 0; i < cachep->objPerSlab; i++){
			void* object = (unsigned char*)freeSlab->s_mem + cachep->objSize*i;
			if (cachep->dtor)
				cachep->dtor(object);
		}

		cachep->slabsFree = freeSlab->next;
		freedBlocks += cachep->numOfBlocks;
		buddyAdd(freeSlab, cachep->numOfBlocks);
		cachep->numOfSlabs--;

		freeSlab = freeSlab->next;
	}

	return freedBlocks;
}

void kmem_cache_free(kmem_cache_t *cachep, void *objp){
	cachep->cacheMutex->lock();
    //mtx.lock();
	_kmem_cache_free(cachep, objp);
	
	cachep->cacheMutex->unlock();
	//mtx.unlock();
}

int _kmem_cache_free(kmem_cache_t *cachep, void *objp){
	if (cachep == 0 || objp == 0) return 0;
	slab_t* slab = cachep->slabsPartial;
	slab_t* prevSlab = 0;
	int found = 0;
	while (1){
		if (!slab){
			break;
		}
		if ((((unsigned char*)slab + cachep->numOfBlocks*BLOCK_SIZE) > (unsigned char*)objp) && ((unsigned char*)slab < (unsigned char*)objp)){
			if (cachep->dtor)
				(*(cachep->dtor))(objp);
			//if (cachep->ctor)
			//	(*(cachep->ctor))(objp);

			slab->inuse--;
			cachep->objInUse--;
			int objInd = ((unsigned char*)objp - (unsigned char*)slab->s_mem) / cachep->objSize;
			if (objInd>100000){
				int b = 1;
			}
			slab_bufctl(slab)[objInd] = slab->free;
			slab->free = objInd;

			//update slab lists
			if (slab->inuse == 0){
				if (prevSlab)
					prevSlab->next = slab->next;
				else
					cachep->slabsPartial = slab->next;
				slab->next = cachep->slabsFree;
				cachep->slabsFree = slab;
			}

			found = 1;
			break;
		}
		else{
			prevSlab = slab;
			slab = slab->next;
		}
	}
	if (!found){
		slab = cachep->slabsFull;
		while (1){
			if (!slab){
				break;
			}
			if ((((unsigned char*)slab + cachep->numOfBlocks*BLOCK_SIZE) > (unsigned char*)objp) && ((unsigned char*)slab < (unsigned char*)objp)){
				if (cachep->dtor)
					(*(cachep->dtor))(objp);
				//if (cachep->ctor)
				//	(*(cachep->ctor))(objp);

				slab->inuse--;
				cachep->objInUse--;
				int objInd = ((unsigned char*)objp - (unsigned char*)slab->s_mem) / cachep->objSize;
				slab_bufctl(slab)[objInd] = slab->free;
				slab->free = objInd;

				if (prevSlab)
					prevSlab->next = slab->next;
				else
					cachep->slabsFull = slab->next;
				slab->next = cachep->slabsPartial;
				cachep->slabsPartial = slab;

				found = 1;
				break;
			}
			else{
				prevSlab = slab;
				slab = slab->next;
			}
		}
	}

	return found;
}

void *kmalloc(size_t size){
	
	//mtx.lock();

	kmem_cache_t* cache;

	if (size > (int)pow(2.0, 17)) return 0;
	int power = ceil(log((double)size) / log(2.0));
	if (power < 5) power = 5;

	cache = cacheManagerPtr->sizesCache[power - 5];

	cache->cacheMutex->lock();
	void* ret = _kmem_cache_alloc(cache);
	cache->cacheMutex->unlock();
	//mtx.unlock();

	return ret;
}

void kfree(const void *objp){
	//mtx.lock();

	int i;
	for (i = 0; i < NUM_OF_MEM_BUFFS; i++){
		kmem_cache_t* cache = cacheManagerPtr->sizesCache[i];
		cache->cacheMutex->lock();
		
		if (_kmem_cache_free(cache, (void*)objp) == 1) {
			
			cache->cacheMutex->unlock();
			break;
		}
		
		cache->cacheMutex->unlock();
		
	}

	//mtx.unlock();
}

void kmem_cache_destroy(kmem_cache_t *cachep){
	mtx.lock();
	//cachep->cacheMutex->lock();

	slab_t* temp;
	while (cachep->slabsPartial){
		temp = cachep->slabsPartial;
		cachep->slabsPartial = temp->next;
		buddyAdd(temp, cachep->numOfBlocks);
	}

	while (cachep->slabsFull){
		temp = cachep->slabsFull;
		cachep->slabsFull = temp->next;
		buddyAdd(temp, cachep->numOfBlocks);
	}
	
	_kmem_cache_shrink(cachep);

	kmem_cache_t* tempCache = cacheManagerPtr->firstCache;
	kmem_cache_t* prevCache = 0;

	while (tempCache != cachep && tempCache != 0){
		prevCache = tempCache;
		tempCache = tempCache->next;
	}

	//if (tempCache == 0) return;
	if (prevCache)
		prevCache->next = tempCache->next;
	else
		cacheManagerPtr->firstCache = tempCache->next;

	cacheManagerPtr->numOfCaches--;

	//cachep->cacheMutex->unlock();

	buddyAdd((void*)tempCache, 1);

	if (cacheManagerPtr->numOfCaches == 0){
		buddyAdd(cacheManagerPtr, 1);
	}

	mtx.unlock();
}

void kmem_cache_info(kmem_cache_t *cachep){
	cachep->cacheMutex->lock();
	//mtx.lock();

	char name[50];
	strcpy(name, cachep->name);
	int totalNumOfBlocks = 0;
	int numOfSlabs=0;
	
	slab_t* temp = cachep->slabsFree;
	while (temp){
		numOfSlabs++;
		temp = temp->next;
	}
	temp = cachep->slabsFull;
	while (temp){
		numOfSlabs++;
		temp = temp->next;
	}
	temp = cachep->slabsPartial;
	while (temp){
		numOfSlabs++;
		temp = temp->next;
	}

	totalNumOfBlocks = numOfSlabs*cachep->numOfBlocks;
	double usage = (((double)cachep->objInUse) / (cachep->numOfSlabs*cachep->objPerSlab)) * 100;
	printf("Name: %s ObjSize: %d CacheSize(blocks): %d NumOfSlabs: %d ObjPerSlab: %d Usage: %.2f ObjInUseInCache: %d\n", name, cachep->objSize, totalNumOfBlocks, numOfSlabs, cachep->objPerSlab, usage, cachep->objInUse);

	cachep->cacheMutex->unlock();
	//mtx.unlock();
}

int kmem_cache_error(kmem_cache_t *cachep){
	return 0;
}