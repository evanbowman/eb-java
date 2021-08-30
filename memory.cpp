#include "memory.hpp"
#include <stdlib.h>



namespace java {
namespace jvm {
namespace heap {



inline void* align(size_t __align,
                   size_t __size,
                   void*& __ptr,
                   size_t& __space) noexcept
{
    const auto __intptr = reinterpret_cast<uintptr_t>(__ptr);
    const auto __aligned = (__intptr - 1u + __align) & -__align;
    const auto __diff = __aligned - __intptr;
    if ((__size + __diff) > __space)
        return nullptr;
    else {
        __space -= __diff;
        return __ptr = reinterpret_cast<void*>(__aligned);
    }
}




static u8* heap_begin;
static u8* heap_end;
static void* heap_alloc;



void init(u32 heap_size)
{
    heap_begin = (u8*)malloc(heap_size);
    heap_end = heap_begin + heap_size;

    heap_alloc = heap_begin;
}



Object* allocate(size_t size)
{
    std::size_t total_heap = heap_end - (u8*)heap_alloc;

    if (align(alignof(Object), size, heap_alloc, total_heap)) {
        printf("heap allocate instance %p, inst size %zu, remaining %zu\n",
               heap_alloc,
               size,
               total_heap - size);
        auto result = heap_alloc;
        heap_alloc = (u8*)heap_alloc + size;
        return (Object*)result;
    }
    return nullptr;
}



} // namespace heap
} // namespace jvm
} // namespace java
