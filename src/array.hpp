#pragma once

#include "object.hpp"
#include "vm.hpp"



namespace java {


struct Array {
    Object object_;
    u32 size_ : 31;
    u32 is_primitive_ : 1;

    enum Type : u8 {
        t_boolean = 4,
        t_char = 5,
        t_float = 6,
        t_double = 7,
        t_byte = 8,
        t_short = 9,
        t_int = 10,
        t_long = 11,
    };

    union {
        struct PrimitiveMetadata {
            u8 element_size_;
            Type type_;
        } primitive_;
        Class* class_type_;
    } metadata_;


    // fields[...]


    u8 element_size() const
    {
        if (is_primitive_) {
            return metadata_.primitive_.element_size_;
        } else {
            return sizeof(Object*);
        }
    }


    // Constructor for arrays of primitive datatypes (int, char, float, etc.)
    Array(int size, u8 element_size, Type primitive_type)
        : object_(nullptr), // Fill in the class pointer later
          size_(size), is_primitive_(1)
    {
        metadata_.primitive_.element_size_ = element_size;
        metadata_.primitive_.type_ = primitive_type;
    }


    // Constructor for arrays of Objects. We need to store special metadata
    // about the type of object stored in the array, so that we can correctly
    // implement checked casts.
    Array(int size, Class* class_type)
        : object_(nullptr), size_(size), is_primitive_(0)
    {
        metadata_.class_type_ = class_type;
    }


    static Array* create(int size, u8 element_size, Type primitive_type);


    static Array* create(int size, Class* class_type);


    size_t memory_footprint() const
    {
        return sizeof(Array) + size_ * element_size();
    }


    u8* data()
    {
        return reinterpret_cast<u8*>(this) + sizeof(Array);
    }


    u8* address(int index)
    {
        return data() + index * element_size();
    }


    bool check_bounds(int index)
    {
        return index >= 0 and index < size_;
    }
};



} // namespace java
