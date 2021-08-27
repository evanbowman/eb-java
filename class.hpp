#pragma once

#include "classfile.hpp"
#include "slice.hpp"



namespace java {



struct Class {
    // Stores an array of pointers into the classfile's constants array. Speeds
    // up constant loading.
    const ClassFile::ConstantHeader** constants_ = nullptr;


    ClassFile::MethodInfo** methods_ = nullptr;
    u16 method_count_ = 0;



    const ClassFile::ConstantHeader* load_constant(u16 index);


    Slice load_string_constant(u16 index);


    const ClassFile::MethodInfo* load_method(const char* name);
};




}
