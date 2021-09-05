#pragma once

#include "class.hpp"
#include "slice.hpp"



namespace java {
namespace jvm {
namespace classtable {



void insert(Slice name, Class* clz);



Class* load(Slice name);



void visit(void (*visitor)(Class*));



}
}
}