#pragma once

#include "int.h"
#include "object.hpp"



#ifndef JVM_HEAP_SIZE
#define JVM_HEAP_SIZE 256000
#endif



namespace java {
namespace jvm {
namespace heap {



Object* allocate(size_t size);



} // namespace heap


namespace classmemory {


// Class Memory, used for class metadata, jni bindings, etc. Nothing in this
// region is intended to ever be deallocated. Grows from the end of the heap,
// i.e. shares memory with class instances.


void* allocate(size_t size, size_t align);



} // namespace classmemory



} // namespace jvm
} // namespace java
