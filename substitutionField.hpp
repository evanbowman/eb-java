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
        b8 = 3,  // eight bytes
        b_invalid = 4
    };

    Size size_ : 3;
    u16 offset_ : 13; // Ok, so we lose three bits of precision, which
                      // technically isn't compliant with the java standard. But
                      // who puts 65535 fields in a class anyway?
};



}
