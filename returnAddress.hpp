#pragma once

#include "object.hpp"



namespace java {
namespace jvm {


// Our JVM implementation assumes that all values used with aload are instances
// of classes. Therefore, the jvm returnAddress datatype must be implemented as
// an instance of a pseudo-class. Newer compilers don't generate jsr/jsr_w/ret
// anyway, so this is only relevant when running legacy jars.
struct ReturnAddress {

    ReturnAddress(Class* clz, u32 pc) : object_(clz), pc_(pc)
    {
    }


    Object object_;
    u32 pc_;
};



} // namespace jvm
} // namespace java
