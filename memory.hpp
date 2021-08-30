#pragma once

#include "int.h"



namespace java {
namespace jvm {
namespace heap {



void init(u32 heap_size);



// When allocating java objects on the heap, use object_malloc. For everything
// else, you need to use the plain malloc function.
void* object_malloc(u32 bytes);
void* malloc(u32 bytes);



} // namespace heap
} // namespace jvm
} // namespace java
