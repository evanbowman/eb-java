#include "memory.hpp"
#include "defines.hpp"
#include "gc.hpp"
#include "vm.hpp"
#include <stdlib.h>
#include <cstdio>



namespace java {
namespace jvm {
namespace heap {



JVM_HEAP_SECTION static u8 heap_[JVM_HEAP_SIZE] alignas(Object);



static u8* heap_end = heap_ + JVM_HEAP_SIZE;
static void* heap_alloc = heap_;



u8* begin()
{
    return heap_;
}



u8* end()
{
    return (u8*)heap_alloc;
}



void __overwrite_end(u8* new_end)
{
    heap_alloc = new_end;
}



size_t total()
{
    return JVM_HEAP_SIZE;
}



size_t used()
{
    return (JVM_HEAP_SIZE -
            (heap_end - heap_))         // class metadata grows from end.
           + ((u8*)heap_alloc - heap_); // object instance grow from beginning.
}



void print_stats(void (*print_str_callback)(const char*))
{
    print_str_callback("Heap Chart   (*) used   (.) unused\n");


    static const int width = 80;
    static const int height = 5;

    const auto im = (u8*)heap_alloc - heap_;
    const auto cm = JVM_HEAP_SIZE - (heap_end - heap_);


    const int total = width * height;
    auto begin = total * (float(im) / JVM_HEAP_SIZE);
    auto end = total * (float(cm) / JVM_HEAP_SIZE);
    auto middle = (total - begin) - end;

    char matrix[width][height];


    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            if (begin > 0) {
                --begin;
                matrix[x][y] = '*';
            } else if (middle > 0) {
                --middle;
                matrix[x][y] = '.';
            } else {
                matrix[x][y] = '*';
            }
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            char buffer[2];
            buffer[0] = matrix[x][y];
            buffer[1] = '\0';
            print_str_callback(buffer);
        }
        print_str_callback("\n");
    }

    char buffer[100];

    snprintf(buffer, sizeof buffer, "objects %zu bytes -->", im);

    print_str_callback(buffer);

    const auto prefix_len = strlen(buffer);

    snprintf(buffer, sizeof buffer, "<-- class info %zu bytes\n", cm);

    const auto suffix_len = strlen(buffer);

    for (int i = 0; i < (int)(81 - (prefix_len + suffix_len)); ++i) {
        print_str_callback(" ");
    }

    print_str_callback(buffer);


    snprintf(buffer,
             sizeof buffer,
             "heap used %zu, remaining "
             "%zu\n",
             used(),
             heap_end - (u8*)heap_alloc);

    print_str_callback(buffer);
}



Object* allocate(size_t size)
{
    size = aligned_size(size);

    auto try_alloc = [&]() -> Object* {
        if (heap_end < heap_alloc) {
            // This should never happen, right?
            while (true)
                ;
        }

        auto remaining = (u8*)heap_end - (u8*)heap_alloc;

        if ((size_t)remaining >= size) {
            auto result = (Object*)heap_alloc;
            heap_alloc = ((u8*)heap_alloc) + size;
            return result;
        }

        return nullptr;
    };

    Object* mem = try_alloc();

    if (mem == nullptr) {
        gc::collect();
    } else {
        return mem;
    }

    return try_alloc();
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
        gc::collect();
    } else {
        heap::heap_end = alloc_ptr;
        return alloc_ptr;
    }


    alloc_ptr = (u8*)heap::heap_end - size;

    while (((size_t)alloc_ptr) % alignment not_eq 0) {
        --alloc_ptr;
    }


    if (alloc_ptr < heap::heap_alloc) {
        unhandled_error("oom!");
    }

    heap::heap_end = alloc_ptr;
    return alloc_ptr;
}



} // namespace classmemory



} // namespace jvm
} // namespace java
