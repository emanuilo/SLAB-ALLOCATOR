# SLAB-ALLOCATOR
Slab allocator implementation for purposes of operating system kernel memory allocation and deallocation.  
* Allocating of small memory buffers in order to decrease internal fragmentation
* Caching of frequently used objects in order to avoid allocation, initialization and destroying objects
* Better utilization of a hardware cache memory by alligning objects to the L1 cache line
