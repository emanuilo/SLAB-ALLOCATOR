#ifndef _buddy_h_
#define _buddy_h_

#define BLOCK_SIZE 4096
#include <mutex>

extern std::mutex mtx;
extern std::mutex buddyMutex;

extern void* base_addr;

typedef union block{
	union block *next;
	unsigned char data[4096];
}block;

typedef struct buddy{
	union block* buddyArray[32];
	int numOfLevels;
}buddy;

void buddyInit(void* space, int blockNum);

void buddyAdd(void *addr, int blockNum);

void _buddyAdd(void *addr, int blockNum);

void* buddyGet(int blockNum);

void* _buddyGet(int blockNum);

#endif