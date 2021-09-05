#include "jni.hpp"
#include "memory.hpp"
#include "methodTable.hpp"
#include "vm.hpp"



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

    // We need a way to bind a native method to this class. If we haven't
    // allocated a method table, we have nowhere to attach the native method. So
    // using the native interface requires allocation of a method table, if one
    // does not yet exist.
    if (not(clz->flags_ & Class::Flag::has_method_table)) {
        clz->flags_ |= Class::Flag::has_method_table;

        puts("allocate method table while binding native method");

        // NOTE: If the has_method_table flag is false, the methods_ field will
        // be assigned to the section of the classfile consisting of the method
        // implementations.
        clz->methods_ = jvm::classmemory::allocate<MethodTableImpl>(
            (const ClassFile::HeaderSection4*)clz->methods_);
    }

    auto stub = (MethodStub*)jvm::classmemory::allocate(sizeof(MethodStub),
                                                        alignof(MethodStub));

    stub->implementation_ = implementation;

    ((MethodTable*)clz->methods_)
        ->bind_native_method(clz, method_name, method_type_signature, stub);
}



} // namespace jni
} // namespace java
