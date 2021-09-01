#pragma once

#include "classfile.hpp"
#include "constantPool.hpp"
#include "slice.hpp"
#include "substitutionField.hpp"



namespace java {



struct Object;



struct Class {

    struct {
        u32 is_class_ : 1;
        u32 reserved_ : 31;
    } flags_;

    ConstantPool* constants_;

    const ClassFile::MethodInfo** methods_ = nullptr;
    Class* super_ = nullptr;
    const char* classfile_data_ = nullptr;

    // We keep track of the highest index of SubstitutionField in our class
    // instance. In doing so, we know how large class instances need to be.
    s16 cpool_highest_field_ = -1;

    u16 method_count_ = 0;


    const ClassFile::HeaderSection2* interfaces() const;


    const ClassFile::MethodInfo* load_method(const char* name,
                                             Slice type_signature);


    void put_field(Object* obj, u16 const_pool_index, void* value);


    void* get_field(Object* obj, u16 const_pool_index, bool& is_object);


    // The extra memory required to hold all fields of an instance of this
    // class.
    size_t instance_fields_size();
};



} // namespace java
