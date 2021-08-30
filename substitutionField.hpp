#pragma once

#include "int.h"


namespace java {



// The classfile's field descriptors don't actually give any indication about
// the field layout in instance of a class. For efficiency, we swap out pointers
// in the constant pool with our own field descriptions, which specify the byte
// offset of the field in the class instance.
struct SubstitutionField {

    enum Size : u16 {
        b1 = 0, // one byte
        b2 = 1, // two bytes
        b4 = 2, // four bytes
        b8 = 3, // eight bytes
        b_invalid = 4,
        b_static = 5, // reserved! We will need to use a completely different
                      // data structure for static vars. We care less about
                      // compactness and speed, in those cases.
        b_ref = 6, // The field is a reference. We care about this property when
                   // pushing fields onto the operand stack, at which point, we
                   // care whether or not the field is a class instance.
    };

    u32 real_size() const
    {
        return (size_ == b_ref) ? sizeof(void*) : (1 << size_);
    }

    Size size_ : 3;
    u16 offset_ : 13; // Ok, so we lose three bits of precision, which
                      // technically isn't compliant with the java standard. But
                      // who puts 65535 fields in a class anyway?
};



} // namespace java
