#include "methodTable.hpp"
#include "crc32.hpp"
#include "memory.hpp"



namespace java {



MethodTableImpl::MethodTableImpl(const ClassFile::HeaderSection4* h4)
{
    auto str = (const char*)h4;
    str += sizeof(ClassFile::HeaderSection4);

    methods_ = (const ClassFile::MethodInfo**)jvm::classmemory::allocate(
        sizeof(ClassFile::MethodInfo*) * h4->methods_count_.get(),
        alignof(ClassFile::MethodInfo));

    if (methods_ == nullptr) {
        while (true)
            ; // TODO: raise error...
    }

    method_count_ = h4->methods_count_.get();

    for (int i = 0; i < h4->methods_count_.get(); ++i) {
        auto method = (ClassFile::MethodInfo*)str;
        str += sizeof(ClassFile::MethodInfo);
        methods_[i] = method;

        for (int i = 0; i < method->attributes_count_.get(); ++i) {
            auto attr = (ClassFile::AttributeInfo*)str;
            str += sizeof(ClassFile::AttributeInfo) +
                   attr->attribute_length_.get();
        }
    }

    for (auto& mtd : method_cache_) {
        mtd = nullptr;
    }
}



const ClassFile::MethodInfo*
MethodTableImpl::load_method(Class* clz, Slice lhs_name, Slice lhs_type)
{
    // TODO: This stuff should really be refactored. A method cache loses some
    // of its effectiveness if it does not cache methods from the
    // superclasses...
    auto index = crc32(lhs_name.ptr_, lhs_name.length_) % JVM_METHOD_CACHE_SIZE;

    if (auto mtd = method_cache_[index]) {
        u16 name_index = mtd->name_index_.get();
        u16 type_index = mtd->descriptor_index_.get();

        auto rhs_name = clz->constants_->load_string(name_index);
        auto rhs_type = clz->constants_->load_string(type_index);

        if (rhs_name == lhs_name and rhs_type == lhs_type) {
            return mtd;
        }
    }


    if (methods_) {
        for (int i = 0; i < method_count_; ++i) {
            u16 name_index = methods_[i]->name_index_.get();
            u16 type_index = methods_[i]->descriptor_index_.get();

            auto rhs_name = clz->constants_->load_string(name_index);
            auto rhs_type = clz->constants_->load_string(type_index);

            if (lhs_type == rhs_type and lhs_name == rhs_name) {
                method_cache_[index] = methods_[i];
                return methods_[i];
            }
        }
    }
    return nullptr;
}



void MethodTableImpl::bind_native_method(Class* clz,
                                         Slice method_name,
                                         Slice type_signature,
                                         jni::MethodStub* stub)
{
    for (int i = 0; i < method_count_; ++i) {
        const auto method_name_str =
            clz->constants_->load_string(methods_[i]->name_index_.get());

        // FIXME: check method type signature too, to avoid issues with
        // overloaded methods.

        if (method_name_str == method_name) {

            auto old_method = methods_[i];

            memcpy(
                &stub->method_info_, old_method, sizeof(ClassFile::MethodInfo));

            stub->method_info_.attributes_count_.set(1);

            // FIXME: Is this safe? We need a way to distinguish between native
            // methods and "Code", so we're using a hard-coded constant.  A
            // classfile's constant pool would need to be massive for this ever
            // to create a problem...
            stub->attribute_info_.attribute_name_index_.set(jni::magic);
            stub->attribute_info_.attribute_length_.set(jni::magic);

            methods_[i] = (ClassFile::MethodInfo*)stub;
            return;
        }
    }
}


void MethodTableImpl::visit_methods(
    Class* clz,
    void (*visitor)(Class*, const ClassFile::MethodInfo*, void*),
    void* arg)
{
    for (int i = 0; i < method_count_; ++i) {
        visitor(clz, methods_[i], arg);
    }
}



} // namespace java
