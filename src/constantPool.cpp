#include "constantPool.hpp"
#include "memory.hpp"
#include "vm.hpp"



namespace java {



const char* ConstantPoolArrayImpl::parse(const ClassFile::HeaderSection1& src)
{
    array_ = (const ClassFile::ConstantHeader**)jvm::classmemory::allocate(
        sizeof(ClassFile::ConstantHeader*) * src.constant_count_.get() - 1,
        alignof(ClassFile::ConstantHeader*));

    const char* str = ((const char*)&src) + sizeof(ClassFile::HeaderSection1);


    for (int i = 0; i < src.constant_count_.get() - 1; ++i) {
        auto c = (const ClassFile::ConstantHeader*)str;

        array_[i] = c;
        str += ClassFile::constant_size((const ClassFile::ConstantHeader*)str);

        if (c->tag_ == ClassFile::t_double or c->tag_ == ClassFile::t_long) {
            ++i;
        }
    }

    return str;
}



void ConstantPoolArrayImpl::reserve_fields(int field_count)
{
    fields_ = (SubstitutionField*)jvm::classmemory::allocate(
        sizeof(SubstitutionField) * field_count, alignof(SubstitutionField));
}



void ConstantPoolArrayImpl::bind_field(u16 index, SubstitutionField field)
{
    *fields_ = field;
    array_[index - 1] = (const ClassFile::ConstantHeader*)fields_;
    ++fields_;
}



void ConstantPoolCompactImpl::reserve_fields(int count)
{
    if (count == 0) {
        return;
    }

    if (bindings_) {
        unhandled_error("reserve fields called twice");
    }

    bindings_ = (FieldBinding*)jvm::classmemory::allocate(
        sizeof(FieldBinding) * count, alignof(FieldBinding));

    memset((void*)bindings_, 0, sizeof(FieldBinding) * count);

    for (int i = 0; i < count; ++i) {
        bindings_[i].field_.valid_ = 0;
    }

    if (bindings_ == nullptr) {
        unhandled_error("alloc failed");
    }

    binding_count_ = count;
}



} // namespace java
