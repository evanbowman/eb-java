#include "memory.hpp"
#include <stdlib.h>



namespace java {
namespace jvm {
namespace heap {



static u8 heap_[JVM_HEAP_SIZE] alignas(Object);


static u8* heap_end = heap_ + JVM_HEAP_SIZE;
static void* heap_alloc = heap_;



static size_t used()
{
    return
        (JVM_HEAP_SIZE - (heap_end - heap_)) // class metadata grows from end.
        + ((u8*)heap_alloc - heap_); // object instance grow from beginning.
}



Object* allocate(size_t size)
{
    while (size % alignof(Object) not_eq 0) {
        ++size;
    }

    auto remaining = heap_end - (u8*)heap_alloc;

    if (remaining) {
        auto result = (Object*)heap_alloc;
        heap_alloc = ((u8*)heap_alloc) + size;

        printf("heap allocate instance %p, inst size %zu, used %zu, remaining %zu\n",
               heap_alloc,
               size,
               used(),
               heap_end - (u8*)heap_alloc);
        
        return result;
    }

    return nullptr;
}



} // namespace heap


namespace classmemory {



void* allocate(size_t size, size_t alignment)
{
    if (size == 0) {
        return heap::heap_end;
    }

    // Remove bytes from the end of the heap, equal to the size of the aligned
    // allocation.

    auto alloc_ptr = (u8*)heap::heap_end - size;

    while (((size_t)alloc_ptr) % alignment not_eq 0) {
        --alloc_ptr;
    }

    if (alloc_ptr < heap::heap_alloc) {
        puts("cm exhausted!");
        while (true) ;
    }


    heap::heap_end = alloc_ptr;

    printf("cm allocate %p, inst size %zu, used: %zu, remaining %zu\n",
           alloc_ptr,
           size,
           heap::used(),
           heap::heap_end - (u8*)heap::heap_alloc);


    return alloc_ptr;
}



}



} // namespace jvm
} // namespace java
