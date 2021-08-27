#include "class.hpp"



namespace java {



const ClassFile::ConstantHeader* Class::load_constant(u16 index)
{
    return constants_[index - 1];
}



Slice Class::load_string_constant(u16 index)
{
    auto constant = load_constant(index);

    if (constant->tag_ == ClassFile::ConstantType::t_utf8) {
        return Slice {
            (const char*)constant + sizeof(ClassFile::ConstantUtf8),
            ((ClassFile::ConstantUtf8*)constant)->length_.get()
        };
    } else {
        while (true) ;
    }
}



const ClassFile::MethodInfo* Class::load_method(const char* name)
{
    auto name_slc = Slice::from_c_str(name);

    if (methods_) {
        for (int i = 0; i < method_count_; ++i) {

            const auto method_name_str =
                load_string_constant(methods_[i]->name_index_.get());

            if (method_name_str == name_slc) {
                return methods_[i];
            }
        }
    }
    return nullptr;
}



}
