#pragma once

#include "class.hpp"
#include "slice.hpp"



namespace java {
namespace jvm {



#ifndef JVM_OPERAND_STACK_SIZE
#define JVM_OPERAND_STACK_SIZE 512
#endif


#ifndef JVM_STACK_LOCALS_SIZE
#define JVM_STACK_LOCALS_SIZE 1024
#endif



// Start the VM, passing in a pointer to jar file contents.
void start(const u8* jar_file_bytes);



void register_class(Slice name, Class* clz);



Class* load_class(Class* current_module, u16 class_index);







} // namespace jvm
} // namespace java
