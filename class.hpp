#pragma once

#include "classfile.hpp"
#include "constantPool.hpp"
#include "slice.hpp"



namespace java {



struct Object;



// The classfile's field descriptors don't actually give any indication about
// the field layout in instance of a class. For efficiency, we swap out pointers
// in the constant pool with our own field descriptions, which specify the byte
// offset of the field in the class instance.
struct SubstitutionField {

    enum Size : u16 {
        b1 = 0, // one byte
        b2 = 1, // two bytes
        b4 = 2, // four bytes
        b8 = 3  // eight bytes (would a jvm datatype ever be larger than eight?)
    };

    Size size_ : 2;
    u16 offset_ : 14;
};



struct Class {
    ConstantPool* constants_;

    const ClassFile::MethodInfo** methods_ = nullptr;
    Class* super_ = nullptr;
    const char* classfile_data_;

    // We keep track of the highest index of SubstitutionField in our class
    // instance. In doing so, we know how large class instances need to be.
    s16 cpool_highest_field_ = -1;

    u16 method_count_ = 0;



    const ClassFile::MethodInfo* load_method(const char* name);


    void put_field(Object* obj, u16 const_pool_index, void* value);


    void* get_field(Object* obj, u16 const_pool_index);


    // The extra memory required to hold all fields of an instance of this
    // class.
    size_t instance_fields_size();
};




}
