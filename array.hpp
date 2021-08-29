#pragma once

#include "object.hpp"
#include "vm.hpp"



namespace java {


struct Array {
    Object object_;
    int size_;
    u8 element_size_;

    // fields[...]


    static Array* create(int size, u8 element_size);


    u8* data()
    {
        return reinterpret_cast<u8*>(this) + sizeof(Array);
    }


    // template <typename T>
    // T load(int index)
    // {
    //     if (index >= 0 and index < size_) {
    //         u8* element = data() + index * element_size_;
    //         T result;
    //         memcpy(&result, element, sizeof(T));
    //         return result;
    //     }
    // }
};



}
