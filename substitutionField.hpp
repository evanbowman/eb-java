#pragma once

#include "endian.hpp"
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
    };

    Size size_ : 2;
    u16 local_ : 1;
    u16 object_ : 1;
    u16 valid_ : 1;

    // Byte offset of the field in the instance of an object. We support object
    // sizes of max 2047 bytes. You can create arrays larger than this, I don't
    // really think most users would need more than 2047 bytes of member fields
    // in a single object.
    u16 offset_ : 11;


    SubstitutionField() : valid_(0)
    {
    }


    SubstitutionField(Size size, u16 offset, bool is_object)
        : size_(size), local_(false), object_(is_object), valid_(1),
          offset_(offset)
    {
    }


    u32 real_size() const
    {
        return 1 << size_;
    }
};
static_assert(sizeof(SubstitutionField) == 2 and
                  alignof(SubstitutionField) == 2,
              "");



} // namespace java
