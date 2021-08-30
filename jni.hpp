#pragma once

#include "class.hpp"



namespace java {
namespace jni {



enum {
    magic = 65535
};



struct MethodStub {
    ClassFile::MethodInfo method_info_;
    ClassFile::AttributeInfo attribute_info_;

    void (*implementation_)();
};




// The simplest way to bind a C++ function to a native method. Technically, if
// you knew all of the classfile offsets, you could built the method stub
// yourself, thus avoiding a malloc.
void bind_native_method(Class* clz,
                        Slice method_name,
                        Slice method_type_signature,
                        void (*implementation)());



}
}
