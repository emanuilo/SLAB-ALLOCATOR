#include "buddy.h"
#include <cmath>
#include <stdio.h>

void* base_addr;
std::mutex buddyMutex;

void buddyInit(void* _baseAddr, int blocks){
	base_addr = _baseAddr;
	int blockNum = blocks;
	double blockSize = BLOCK_SIZE;
	int power = log(blockSize) / log(2.0);
	unsigned bit = 1 << power;

	unsigned mask = 0xFFFFFFFF << power;
	unsigned newAddress = mask & (unsigned)base_addr;
	if (newAddress != (unsigned)base_addr){
		newAddress = newAddress + bit;
		blockNum--;
		base_addr = (void*)newAddress;
	}

	int actualArraySize = ceil(log((double)blockNum) / log(2.0)) + 1;


	//init
	buddy* buddyPtr = (buddy*)base_addr;
	buddyPtr->numOfLevels = actualArraySize;
	int i;
	for (i = 0; i < actualArraySize; i++){
		buddyPtr->buddyArray[i] = 0;
	}

	blockNum--;
	block* temp = (block*)base_addr + 1; //address of 2nd block

	for (i = 0; i < blockNum; i++){
		temp->next = 0;
		_buddyAdd(temp, 1);
		temp++;
	}

	//test
	for (i = 0; i < actualArraySize; i++){
		temp = buddyPtr->buddyArray[i];
		while (temp){
			int br = pow(2.0, (i));
			printf("%d ", br);
			temp = temp->next;
		}
	}

	/*buddyGet(1);
	buddyGet(100);

	for (i = 0; i < actualArraySize; i++){
		temp = buddyPtr->buddyArray[i];
		while (temp){
			int br = pow(2.0, (i));
			printf("%d ", br);
			temp = temp->next;
		}
	}*/

}

void buddyAdd(void* _addr, int _blockNum){
	buddyMutex.lock();
	_buddyAdd(_addr, _blockNum);
	buddyMutex.unlock();
}

void _buddyAdd(void *_addr, int _blockNum){
	void* addr = _addr;
	int blockNum = _blockNum;
	buddy* _buddy = (buddy*)base_addr;
	//int br = 0;

	while (1){
		//br++;
		int level = log(blockNum) / log(2.0);
		if (_buddy->buddyArray[level] == 0){
			_buddy->buddyArray[level] = (block*)addr;
			break;
		}
		else{
			unsigned char* bla = (unsigned char*)addr;
			if (blockNum == 0){
				int blasd = 1;
			}
			int index = ((unsigned char*)addr - (unsigned char*)base_addr) / ((blockNum)*BLOCK_SIZE);
			int buddyIndex;
			if (index % 2 == 0) buddyIndex = index + 1;
			else buddyIndex = index - 1;
			block* buddyAddress = (block*)(buddyIndex*blockNum*BLOCK_SIZE + (unsigned char*)base_addr);

			block* temp = _buddy->buddyArray[level];
			block* prev = 0;
			int found = 0;
			while (1){
				if (temp == 0) break;
				if (temp == buddyAddress){
					found = 1;
					if (index % 2 == 0){
						//addr stays the same
						blockNum *= 2;
					}
					else{
						addr = (void*)buddyAddress;
						blockNum *= 2;
					}
					if (prev)
						prev->next = temp->next;
					if (temp == _buddy->buddyArray[level])
						_buddy->buddyArray[level] = temp->next;
					temp->next = 0;
					break;
				}
				prev = temp;
				temp = temp->next;
			}
			if (!found){
				((block*)addr)->next = _buddy->buddyArray[level];
				_buddy->buddyArray[level] = (block*)addr;
				break;
			}
		}
	}
}

void* buddyGet(int _blockNum){
	buddyMutex.lock();
	void* ret = _buddyGet(_blockNum);
	buddyMutex.unlock();

	return ret;
}

void* _buddyGet(int _blockNum){
	int blockNum = pow(2.0, ceil((double)(log((double)_blockNum)) / log(2.0)));
	int wantedLevel = log(blockNum) / log(2.0);
	buddy* _buddy = (buddy*)base_addr;

	int tempLevel = wantedLevel;
	while (1){
		if (_buddy->buddyArray[tempLevel] == 0) {
			tempLevel++;
			continue;
		}

		block* _block = _buddy->buddyArray[tempLevel];
		_buddy->buddyArray[tempLevel] = _buddy->buddyArray[tempLevel]->next;
		_block->next = 0;
		while (1){
			int numOfBlocks = pow(2.0, tempLevel);
			if (blockNum != numOfBlocks){
				block* secondHalfAddr = (block*)((unsigned char*)_block + numOfBlocks * BLOCK_SIZE / 2);
				_buddyAdd(secondHalfAddr, numOfBlocks / 2);
				tempLevel--;
			}
			else break;
		}
		return _block;
	}
}