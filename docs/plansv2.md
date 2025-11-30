# Memory

Memory is a limited resource that an operating system must manage.

Before a process can be admitted, it has to have allocated and access to memory in order to function, e.g.:

- Code section for the program
- Data section for global variables
- Stack section for functions and local variables
- Process Contral Block and Executing Args information on higher memory level
- Heap to store Classes and Objects and File Acess

Therefore, before a process should be admitted, we should take a look at how memory should be allocated.

## Memory Allocation

When you want to access a region of memory, you need to have an address.

### Contiguous Memory Allocation

This is a technique where a process is assigned one singular, continuous block of memory.

> One process, one block.

This one simple approach has issues because of the emergent nature of memory allocation.

When I say emergent, it because of three factors:

- one process, one memory block local to an area
- memory blocks are exclusive to one process - no other can touch and cannot be moved
- a process have unpredictable lifetimes - one process can leave and go as time progress

As processes leave and free memory, it creates spots in memory - not continuous.
Remember, a process must have **continuous** memory - so larger processes cannot fit.

This problem is called **fragmentation**. Specifically, external fragmentation, when freed memory is scattered such that it denies memory allocation of processes.

This is a core issue computers face when allocating memory.

## Paging

To combat external fragmentation, we can chop up continuous space of proceses into individual small pages. Same goes for those present in the physical chip now called frames.

By splitting memory, we can fit large processes into the many holes of memory that exist in RAM. If a process wants to access a certain region of memory with an address, the OS translates  the address given by the process to the actual location found in RAM.

The address the process has is called the logical address, while the actual location is called the physical address.

To the process's view, memory looks to be continuous even if one region of memory is split into muliple areas.

This is called paging, dividing a process memory into pages, where each page points onto fixed-blocks of frames within RAM.


To keep track of pages, the OS keeps a page table, a mapping of pages and frames and there are.

## Internal fragmentation and Thrashing

There is an issue that arises when you use pages and frames - interal fragmentation.

Internal fragmentation comes from wasted space within frames. When a process allocates 3.5 KB of memory, but is given 4 KB, then it wastes 0.5 KB of free space.

We cannot allocate smaller pages, since that would lead to thrashing.

Smaller pages would mean there is more range of pages that the TLB (page cache) has to account for even if they point to the same process.

Smaller pages, More overhead.