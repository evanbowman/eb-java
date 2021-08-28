#pragma once

#include "slice.hpp"
#include "class.hpp"



namespace java {
namespace jvm {



void register_class(Slice name, Class* clz);



Class* load_class(Class* current_module, u16 class_index);



void* malloc(size_t);



}
}
