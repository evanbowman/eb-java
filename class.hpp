#pragma once

#include "classfile.hpp"
#include "slice.hpp"



namespace java {



struct Class {

    const ClassFile::ConstantHeader** constants_ = nullptr;
    const ClassFile::MethodInfo** methods_ = nullptr;
    Class* super_ = nullptr;
    u16 field_count_ = 0;
    u16 method_count_ = 0;


    const ClassFile::ConstantHeader* load_constant(u16 index);


    Slice load_string_constant(u16 index);


    const ClassFile::MethodInfo* load_method(const char* name);
};




}
