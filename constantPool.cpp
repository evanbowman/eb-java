#include "constantPool.hpp"
#include "vm.hpp"




namespace java {



const char* ConstantPoolArrayImpl::parse(const ClassFile::HeaderSection1& src)
{
    array_ = (const ClassFile::ConstantHeader**)
        jvm::malloc(sizeof(ClassFile::ConstantHeader*) *
                    src.constant_count_.get() - 1);

    const char* str =
        ((const char*)&src) + sizeof(ClassFile::HeaderSection1);


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
        while (true) ;
    }

    bindings_ = (FieldBinding*)jvm::malloc(sizeof(FieldBinding) * count);

    memset(bindings_, 0, sizeof(FieldBinding) * count);

    if (bindings_ == nullptr) {
        puts("malloc failed!");
        while (true) ;
    }

    binding_count_ = count;
}




}
