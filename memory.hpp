#pragma once

#include "int.h"
#include "object.hpp"



namespace java {
namespace jvm {
namespace heap {



void init(u32 heap_size);



Object* allocate(size_t size);



} // namespace heap
} // namespace jvm
} // namespace java
