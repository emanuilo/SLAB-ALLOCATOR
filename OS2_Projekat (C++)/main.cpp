#include "buddy.h"
#include "slab.h"
#include "cache.h"
#include <stdio.h>
#include <math.h>

void main(){
	char c;
	void* pointer;

	printf("velicina pointera %d\n", sizeof(pointer));
	printf("velicina chara %d\n", sizeof(c));
	printf("velicina chara* %d\n", sizeof(unsigned char*));
	printf("velicina unsigned %d\n", sizeof(unsigned));
	printf("velicina usnigned* %d\n", sizeof(unsigned*));

	int bla = log(4096.0) / log(2.0);
	printf("log2 od 4096 je %d\n", bla);

	printf("\nvelicina unije %d\n", sizeof(block));

	void* baseMem = malloc(1000 * BLOCK_SIZE);

	//buddyInit(baseMem, 1000);

	printf("\nvelicina slab %d, velicina int %d\n", sizeof(slab_t), sizeof(int));

	kmem_init(baseMem, 1000);

	kmem_cache_t* cache = kmem_cache_create("blah", 100, 0, 0);
	void* obj = kmem_cache_alloc(cache);

}