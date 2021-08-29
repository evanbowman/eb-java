#include "array.hpp"
#include "class.hpp"



namespace java {



Array* Array::create(int size, u8 element_size)
{
    auto alloc_size = sizeof(Array) + size * element_size;
    auto mem = (Array*)jvm::malloc(alloc_size);
    memset(mem, 0, alloc_size);

    new (mem) Array;
    mem->size_ = size;
    mem->element_size_ = element_size;
    return mem;
}



}
