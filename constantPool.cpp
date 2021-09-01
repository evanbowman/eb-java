#include "constantPool.hpp"
#include "memory.hpp"
#include "vm.hpp"



namespace java {



const char* ConstantPoolArrayImpl::parse(const ClassFile::HeaderSection1& src)
{
    // FIXME: this dead code does not account for double/long taking up two
    // spaces in the constant pool. needs to be fixed for if/when we resurrect
    // this stuff.

    array_ = (const ClassFile::ConstantHeader**)jvm::classmemory::allocate(
        sizeof(ClassFile::ConstantHeader*) * src.constant_count_.get() - 1,
        alignof(ClassFile::ConstantHeader*));

    const char* str = ((const char*)&src) + sizeof(ClassFile::HeaderSection1);


    for (int i = 0; i < src.constant_count_.get() - 1; ++i) {
        array_[i] = (const ClassFile::ConstantHeader*)str;
        str += ClassFile::constant_size((const ClassFile::ConstantHeader*)str);
    }

    return str;
}



void ConstantPoolCompactImpl::reserve_fields(int count)
{
    if (bindings_) {
        puts("error: reserve_fields!");
        while (true)
            ;
    }

    bindings_ = (FieldBinding*)jvm::classmemory::allocate(
        sizeof(FieldBinding) * count, alignof(FieldBinding));

    memset(bindings_, 0, sizeof(FieldBinding) * count);

    for (int i = 0; i < count; ++i) {
        bindings_[i].field_.size_ = SubstitutionField::Size::b_invalid;
    }

    if (bindings_ == nullptr) {
        puts("malloc failed!");
        while (true)
            ;
    }

    binding_count_ = count;
}



} // namespace java
