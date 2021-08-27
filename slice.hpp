#pragma once

#include <string.h>
#include <algorithm>



namespace java {



struct Slice {
    const char* ptr_ = nullptr;
    size_t length_ = 0;


    static Slice from_c_str(const char* c_str)
    {
        return { c_str, strlen(c_str) };
    }


    bool operator==(const Slice& other) const
    {
        return strncmp(other.ptr_, ptr_, std::min(length_, other.length_)) == 0;
    }
};




}
