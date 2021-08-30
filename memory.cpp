#include "memory.hpp"
#include <stdlib.h>



namespace java {
namespace jvm {
namespace heap {


static u8* heap_begin;
static u8* heap_end;
static u8* heap_alloc;


void init(u32 heap_size)
{
    heap_begin = (u8*)malloc(heap_size);
    heap_end = heap_begin + heap_size;

    heap_alloc = heap_begin;
}



void* object_malloc(u32 bytes)
{
    return ::malloc(bytes);
}



// We're storing both Java objects and JVM datastructures within the same
// contiguous allocation. We need to be able to tell them apart.
struct AllocationHeader {
    void* class_;
};



void* malloc(u32 bytes)
{
    return ::malloc(bytes);
}



} // namespace heap
} // namespace jvm
} // namespace java
