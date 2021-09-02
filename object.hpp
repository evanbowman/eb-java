#pragma once

#include "class.hpp"



namespace java {



struct Object {

    enum Flag { is_class = (1 << 0) };

    u32 flags_;

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
