#pragma once

#include "classfile.hpp"
#include "constantPool.hpp"
#include "slice.hpp"
#include "substitutionField.hpp"
#include "bitvector.hpp"



namespace java {



struct Object;



struct Class {

    // We use the Option class for attaching Optional features to a class. Many
    // classes will not need any extra data.
    struct Option {
        Option* next_;
        enum Type {
            bootstrap_methods,
            static_field,
        };
    };


    enum Flag {
        is_class         = (1 << 0),
        has_method_table = (1 << 1),
        has_options      = (1 << 2),
    };

    u16 flags_ = 0;

    // We keep track of the highest index of SubstitutionField in our class
    // instance. In doing so, we know how large class instances need to be.
    s16 cpool_highest_field_ = -1;


    ConstantPool* constants_ = nullptr;

    // methods_ may point to the section of the classfile with the method
    // implementations, if the has_method_table flags is not set, otherwise,
    // methods_ will contain a pointer to a MethodTable objects.
    void* methods_ = nullptr;

    // Assigned if the class has a superclass. All classes have superclasses in
    // our implementation except for java/lang/Object.
    Class* super_ = nullptr;

    // Points to the start of the classfile.
    const char* classfile_data_ = nullptr;




    const ClassFile::HeaderSection2* interfaces() const;


    const ClassFile::MethodInfo* load_method(Slice method_name,
                                             Slice type_signature);


    void put_field(Object* obj, u16 const_pool_index, void* value);


    void* get_field(Object* obj, u16 const_pool_index, bool& is_object);


    // The extra memory required to hold all fields of an instance of this
    // class. A size of an instance of this class equals sizeof(Object) +
    // class->instance_fields_size().
    size_t instance_fields_size();
};



} // namespace java
