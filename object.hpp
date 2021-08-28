#pragma once

#include "class.hpp"



namespace java {



struct Object {
    Class* class_;
    u8 info_;
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



}
