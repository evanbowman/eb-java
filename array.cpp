#include "array.hpp"
#include "class.hpp"
#include "memory.hpp"



namespace java {



Array* Array::create(int size, u8 element_size)
{
    static_assert(alignof(Array) == alignof(Object),
                  "unsupported alignment");

    auto alloc_size = sizeof(Array) + size * element_size;
    auto mem = (Array*)jvm::heap::allocate(alloc_size);

    if (mem == nullptr) {
        puts("TODO: oom");
        while (true) ;
    }

    memset(mem, 0, alloc_size);

    new (mem) Array;
    mem->size_ = size;
    mem->element_size_ = element_size;
    return mem;
}



} // namespace java
