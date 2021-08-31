#include "jni.hpp"
#include "vm.hpp"
#include "memory.hpp"



namespace java {
namespace jni {


void bind_native_method(Class* clz,
                        Slice method_name,
                        Slice method_type_signature,
                        void (*implementation)())
{
    if (not clz->methods_) {
        return;
    }

    for (int i = 0; i < clz->method_count_; ++i) {
        const auto method_name_str =
            clz->constants_->load_string(clz->methods_[i]->name_index_.get());

        // FIXME: check method type signature too, to avoid issues with
        // overloaded methods.

        if (method_name_str == method_name) {
            auto stub = (MethodStub*)
                jvm::classmemory::allocate(sizeof(MethodStub), alignof(MethodStub));

            auto old_method = clz->methods_[i];

            memcpy(
                &stub->method_info_, old_method, sizeof(ClassFile::MethodInfo));

            stub->method_info_.attributes_count_.set(1);

            // FIXME: Is this safe? We need a way to distinguish between native
            // methods and "Code", so we're using a hard-coded constant.  A
            // classfile's constant pool would need to be massive for this ever
            // to create a problem...
            stub->attribute_info_.attribute_name_index_.set(magic);
            stub->attribute_info_.attribute_length_.set(magic);
            stub->implementation_ = implementation;

            clz->methods_[i] = (ClassFile::MethodInfo*)stub;

            return;
        }
    }
}



} // namespace jni
} // namespace java
