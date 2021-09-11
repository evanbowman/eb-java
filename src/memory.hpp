#pragma once

#include "int.h"
#include "object.hpp"



namespace java {
namespace jvm {
namespace heap {



u8* begin();



u8* end();



// Only inteded to be called by the garbage collector, which, after running,
// needs to overwrite the new allocation pointer.
void __overwrite_end(u8* new_end);



void print_stats(void (*print_str_callback)(const char*));



size_t used();



size_t total();



Object* allocate(size_t size);



inline size_t aligned_size(size_t size)
{
    auto diff = size % alignof(Object);
    if (diff not_eq 0) {
        size += alignof(Object) - diff;
    }
    return size;
}



} // namespace heap


namespace classmemory {


// Class Memory, used for class metadata, jni bindings, etc. Nothing in this
// region is intended to ever be deallocated. Grows from the end of the heap,
// i.e. shares memory with class instances.


void* allocate(size_t size, size_t align);



template <typename T, typename... Args> T* allocate(Args&&... args)
{
    if (auto mem = (T*)allocate(sizeof(T), alignof(T))) {
        new (mem) T(std::forward<Args>(args)...);
        return mem;
    }
    return nullptr;
}



} // namespace classmemory



} // namespace jvm
} // namespace java
