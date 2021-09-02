#pragma once


#include "classfile.hpp"
#include "jni.hpp"



namespace java {



struct Class;



class MethodTable {
public:
    virtual ~MethodTable() {}

    virtual const ClassFile::MethodInfo* load_method(Class* clz,
                                                     Slice method_name,
                                                     Slice type_signature) = 0;

    virtual void bind_native_method(Class* clz,
                                    Slice method_name,
                                    Slice type_signature,
                                    jni::MethodStub* stub) = 0;
};



class MethodTableImpl : public MethodTable {
public:

    MethodTableImpl(const ClassFile::HeaderSection4* classfile_data);


    const ClassFile::MethodInfo* load_method(Class* clz,
                                             Slice method_name,
                                             Slice type_signature) override;


    void bind_native_method(Class* clz,
                            Slice method_name,
                            Slice type_signature,
                            jni::MethodStub* stub) override;

private:
    const ClassFile::MethodInfo** methods_ = nullptr;
    u16 method_count_ = 0;
};



}
