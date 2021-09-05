#pragma once

#include "class.hpp"



namespace java {
namespace jni {



enum { magic = 65535 };



struct MethodStub {
    ClassFile::MethodInfo method_info_;
    ClassFile::AttributeInfo attribute_info_;

    void (*implementation_)(); // Return the number of operand stack slots used
                               // by function result.
};



void bind_native_method(Class* clz,
                        Slice method_name,
                        Slice method_type_signature,
                        void (*implementation)());



} // namespace jni
} // namespace java
