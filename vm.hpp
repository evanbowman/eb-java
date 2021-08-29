#pragma once

#include "slice.hpp"
#include "class.hpp"



namespace java {
namespace jvm {



// Start the VM, passing in a pointer to jar file contents.
void start(const u8* jar_file_bytes);



void register_class(Slice name, Class* clz);



Class* load_class(Class* current_module, u16 class_index);



void* malloc(size_t);



}
}
