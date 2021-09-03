#pragma once

#include "class.hpp"



namespace java {



struct Object {

    struct Header {

        Header() : gc_mark_bit_(0)
        {
        }

        // Used during GC tracing to identify live objects.
        u32 gc_mark_bit_ : 1;

        // Unused bits.
        u32 unused_ : 3;

        // Forwarding pointer, used by the GC when moving objects around in
        // memory. While running the GC, all object references must be replaced
        // by Heap::begin() + object->gc_forwarding_offset_. The number bits
        // reserved for gc_forwarding_offset_ is somewhat arbitrary, although
        // using fewer bits would limit the amount of addressable memory
        // supported by the VM. To address all of the default 256 kilobyte heap,
        // you would only need an 18-bit forwarding offset.
        u32 gc_forwarding_offset_ : 28;
    } header_;


    Object(Class* clz) :
        class_(clz)
    {
    }


    static_assert(sizeof(Header) == 4, "Header does not match expected size");


    Class* class_;


    // After the object header, all the fields will be packed into the end of
    // the object, such that there are no gaps between fields.
    // fields_[...]


    void put_field(u16 const_pool_index, void* value)
    {
        class_->put_field(this, const_pool_index, value);
    }


    void* get_field(u16 const_pool_index, bool& is_object)
    {
        return class_->get_field(this, const_pool_index, is_object);
    }
};



} // namespace java
