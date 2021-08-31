#pragma once

#include "int.h"
#include "object.hpp"



namespace java {
namespace jvm {
namespace heap {


// The heap. Memory used for allocating instances of classes.


void init(u32 heap_size);



Object* allocate(size_t size);



} // namespace heap


namespace classmemory {


// Class Memory, used for class metadata, jni bindings, etc. Nothing in this
// region is intended to ever be deallocated. We can pack things in tighter,
// knowing that we don't need to delete anything.
//
// TODO: We could simply put classmemory at the end of the heap, and have it
// grow in the opposite direction of object instances...



void init(u32 size);



void* allocate(size_t size, size_t align);



}



} // namespace jvm
} // namespace java
