#pragma once

#include "buffer.hpp"
#include "class.hpp"
#include "slice.hpp"
#include "java.hpp"
#include "defines.hpp"



namespace java {
namespace jvm {



enum class OperandTypeCategory {
    object,
    primitive,
    primitive_wide,
};



using OperandStack = Buffer<void*, JVM_OPERAND_STACK_SIZE>;
using OperandTypes = Buffer<OperandTypeCategory, JVM_OPERAND_STACK_SIZE>;


OperandStack& operand_stack();
OperandTypes& operand_types();


using Locals = Buffer<void*, JVM_STACK_LOCALS_SIZE>;
using LocalTypes = Buffer<OperandTypeCategory, JVM_STACK_LOCALS_SIZE>;


Locals& locals();
LocalTypes& local_types();



int start_from_classfile(const char* class_file_bytes, Slice classpath);
int start_from_jar(const char* jar_file_bytes, Slice classpath);



void register_class(Slice name, Class* clz);



Class* load_class(Class* current_module, u16 class_index);
Class* load_class_by_name(Slice class_name);



} // namespace jvm
} // namespace java
