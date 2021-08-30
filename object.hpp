#pragma once

#include "class.hpp"



namespace java {



struct Object {
    Class* class_;
    u32 flags_;

    // After the object header, all the fields will be packed into the end of
    // the object, such that there are no gaps between fields.
    // fields_[...]


    void put_field(u16 const_pool_index, void* value)
    {
        class_->put_field(this, const_pool_index, value);
    }


    void* get_field(u16 const_pool_index)
    {
        return class_->get_field(this, const_pool_index);
    }
};



} // namespace java
