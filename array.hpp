#pragma once

#include "object.hpp"
#include "vm.hpp"



namespace java {


struct Array {
    Object object_;
    int size_;
    u8 element_size_;

    enum Type : u8 {
        t_boolean = 4,
        t_char = 5,
        t_float = 6,
        t_double = 7,
        t_byte = 8,
        t_short = 9,
        t_int = 10,
        t_long = 11,
    } primitive_type_;



    // fields[...]


    Array(int size, u8 element_size)
        : object_(nullptr), // Fill in the class pointer later
          size_(size), element_size_(element_size)
    {
    }


    static Array* create(int size, u8 element_size);


    size_t memory_footprint() const
    {
        return sizeof(Array) + size_ * element_size_;
    }


    u8* data()
    {
        return reinterpret_cast<u8*>(this) + sizeof(Array);
    }


    u8* address(int index)
    {
        return data() + index * element_size_;
    }


    bool check_bounds(int index)
    {
        return index >= 0 and index < size_;
    }
};



} // namespace java
