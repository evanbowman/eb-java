#pragma once

#include "object.hpp"
#include "vm.hpp"



namespace java {


struct Array {
    Object object_;
    int size_;
    u8 element_size_;

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
