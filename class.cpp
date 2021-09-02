#include "class.hpp"
#include "methodTable.hpp"
#include "object.hpp"
#include <stdio.h>



namespace java {



Class::OptionStaticField* Class::lookup_static(u16 index)
{
    auto ref = (const ClassFile::ConstantRef*)constants_->load(index);
    auto nt = (const ClassFile::ConstantNameAndType*)constants_->load(
        ref->name_and_type_index_.get());
    auto name = constants_->load_string(nt->name_index_.get());
    return lookup_static(name);
}


const ClassFile::HeaderSection2* Class::interfaces() const
{
    auto str = classfile_data_;
    auto h1 = (const ClassFile::HeaderSection1*)classfile_data_;
    str += sizeof(ClassFile::HeaderSection1);

    for (int i = 0; i < h1->constant_count_.get() - 1; ++i) {
        str += ClassFile::constant_size((const ClassFile::ConstantHeader*)str);
    }

    return (ClassFile::HeaderSection2*)str;
}



const ClassFile::MethodInfo* Class::load_method(Slice method_name,
                                                Slice type_signature)
{
    if (not(flags_ & Flag::has_method_table)) {
        // We can run without a method table, by searching the raw classfile.

        auto methods = (const ClassFile::HeaderSection4*)methods_;

        const auto method_count = methods->methods_count_.get();

        const char* str = (const char*)methods;
        str += sizeof(ClassFile::HeaderSection4);

        for (int i = 0; i < method_count; ++i) {
            auto method = (const ClassFile::MethodInfo*)str;

            const auto method_name_str =
                constants_->load_string(method->name_index_.get());

            if (method_name_str == method_name) {
                const auto method_type_str =
                    constants_->load_string(method->descriptor_index_.get());

                if (method_type_str == type_signature) {
                    return method;
                }
            }

            str += sizeof(ClassFile::MethodInfo);
            for (int i = 0; i < method->attributes_count_.get(); ++i) {
                auto attr = (ClassFile::AttributeInfo*)str;
                str += sizeof(ClassFile::AttributeInfo) +
                       attr->attribute_length_.get();
            }
        }
    } else {
        auto method_table = (MethodTable*)methods_;
        return method_table->load_method(this, method_name, type_signature);
    }

    return nullptr;
}



void* Class::get_field(Object* obj, u16 index, bool& is_object)
{
    auto c = constants_->load(index);

    auto sub = (SubstitutionField*)c;

    u8* obj_ram = ((u8*)obj) + sizeof(Object);

    // printf("(get field) index %d from %p\n", index, obj_ram);
    if (sub->size_ == SubstitutionField::b_ref) {
        is_object = true;
        Object* val;
        memcpy(&val, obj_ram + sub->offset_, sizeof(Object*));
        return val;
    } else {
        is_object = false;
    }

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
        while (true)
            ;
        break;
    }

    puts("invalid field configuration");
    while (true)
        ;
}



void Class::put_field(Object* obj, u16 index, void* value)
{
    auto c = constants_->load(index);

    auto sub = (SubstitutionField*)c;

    // printf("index %d\n", index);

    u8* obj_ram = ((u8*)obj) + sizeof(Object);

    if (sub->size_ == SubstitutionField::b_ref) {
        memcpy(obj_ram + sub->offset_, &value, sizeof(Object*));
        return;
    }

    switch (1 << sub->size_) {
    case 1:
        obj_ram[sub->offset_] = (u8)(intptr_t)value;
        break;

    case 2: {
        s16 val = (s16)(intptr_t)value;
        memcpy(obj_ram + sub->offset_, &val, sizeof val);
        break;
    }

    case 4: {
        s32 val = (s32)(intptr_t)value;
        memcpy(obj_ram + sub->offset_, &val, sizeof val);
        break;
    }

    case 8:
        puts("TODO: implement eight byte fields!");
        while (true)
            ;
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
            auto sub = (SubstitutionField*)current->constants_->load(
                current->cpool_highest_field_);

            return sub->offset_ + sub->real_size();

        } else {
            current = current->super_;
        }
    }

    return 0;
}



} // namespace java
