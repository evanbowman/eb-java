#include "class.hpp"
#include "object.hpp"
#include <stdio.h>



namespace java {



const ClassFile::HeaderSection2* Class::interfaces() const
{
    auto str = classfile_data_;
    auto h1 = (const ClassFile::HeaderSection1*)classfile_data_;
    str += sizeof(ClassFile::HeaderSection1);

    for (int i = 0; i < h1->constant_count_.get() - 1; ++i) {
        str +=
            ClassFile::constant_size((const ClassFile::ConstantHeader*)
                                     str);
    }

    return (ClassFile::HeaderSection2*)str;
}



const ClassFile::MethodInfo* Class::load_method(const char* name)
{
    auto name_slc = Slice::from_c_str(name);

    if (methods_) {
        for (int i = 0; i < method_count_; ++i) {

            const auto method_name_str =
                constants_->load_string(methods_[i]->name_index_.get());

            if (method_name_str == name_slc) {
                return methods_[i];
            }
        }
    }
    return nullptr;
}



void* Class::get_field(Object* obj, u16 index)
{
    auto c = constants_->load(index);

    auto sub = (SubstitutionField*)c;

    u8* obj_ram = ((u8*)obj) + sizeof(Object);

    // printf("(get field) index %d from %p\n", index, obj_ram);


    switch (1 << sub->size_) {
    case 1:
        return (void*)(intptr_t)obj_ram[sub->offset_];

    case 2: {
        u16 val;
        memcpy(&val, obj_ram + sub->offset_, 2);
        return (void*)(intptr_t)val;
    }

    case 4: {
        u32 val;
        memcpy(&val, obj_ram + sub->offset_, 4);
        return (void*)(intptr_t)val;
    }

    case 8:
        puts("TODO: implement eight byte fields!");
        while (true) ;
        break;
    }

    puts("invalid field configuration");
    while (true) ;
}



void Class::put_field(Object* obj, u16 index, void* value)
{
    auto c = constants_->load(index);

    auto sub = (SubstitutionField*)c;

    // printf("index %d\n", index);

    u8* obj_ram = ((u8*)obj) + sizeof(Object);

    switch (1 << sub->size_) {
    case 1:
        obj_ram[sub->offset_] = (u8)(intptr_t)value;
        break;

    case 2:
        memcpy(obj_ram + sub->offset_, &value, 2);
        break;

    case 4:
        memcpy(obj_ram + sub->offset_, &value, 4);
        break;

    case 8:
        puts("TODO: implement eight byte fields!");
        while (true) ;
        break;
    }

    // printf("put field %d %d %d %p \n", sub->offset_,
    //        1 << sub->size_,
    //        (int)(intptr_t)value,
    //        obj_ram);
}



size_t Class::instance_fields_size()
{
    Class* current = this;

    while (current) {
        if (current->cpool_highest_field_ not_eq -1) {
            auto sub = (SubstitutionField*)
                current->constants_->load(current->cpool_highest_field_);

            return sub->offset_ + (1 << sub->size_);

        } else {
            current = current->super_;
        }
    }

    return 0;
}



}
