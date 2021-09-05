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
    if ((flags_ & Flag::implements_interfaces) == 0) {
        return nullptr;
    }

    auto src = (const ClassFile::HeaderSection1*)classfile_data_;

    const char* str = ((const char*)src) + sizeof(ClassFile::HeaderSection1);

    for (int i = 0; i < src->constant_count_.get() - 1; ++i) {
        auto c = (const ClassFile::ConstantHeader*)str;

        str += ClassFile::constant_size(c);

        if (c->tag_ == ClassFile::t_double or c->tag_ == ClassFile::t_long) {
            ++i;
        }
    }

    return (ClassFile::HeaderSection2*)str;
}



const ClassFile::MethodInfo* Class::load_method(Slice method_name,
                                                Slice type_signature)
{
    if (methods_ == nullptr) {
        return nullptr;
    }

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



size_t Class::instance_size()
{
    return sizeof(Object) + instance_fields_size();
}



} // namespace java
