#pragma once

#include <algorithm>
#include <string.h>



namespace java {



struct Slice {
    const char* ptr_ = nullptr;
    size_t length_ = 0;


    Slice()
    {
    }


    Slice(const char* ptr, size_t length) : ptr_(ptr), length_(length)
    {
    }


    static Slice from_c_str(const char* c_str)
    {
        return Slice(c_str, strlen(c_str));
    }


    bool operator==(const Slice& other) const
    {
        return length_ == other.length_ and
               strncmp(other.ptr_, ptr_, length_) == 0;
    }
};



} // namespace java
