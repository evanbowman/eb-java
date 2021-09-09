#pragma once

#include "class.hpp"
#include "slice.hpp"



namespace java {
namespace jvm {
namespace classtable {



void insert(Slice name, Class* clz);



Class* load(Slice name);



void visit(void (*visitor)(Slice, Class*, void*), void* arg);



// NOTE: may be very slow.
Slice name(Class* clz);



int size();



} // namespace classtable
} // namespace jvm
} // namespace java
