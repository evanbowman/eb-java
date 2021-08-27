#include "classfile.hpp"
#include "class.hpp"
#include "endian.hpp"
#include <iostream>
#include <map>
#include <string.h>
#include <vector>



const char* get_file_contents(const char* file);



namespace java {




struct Object {
    Class* class_;
    // instance_vars_[...]
};



namespace jvm {



std::vector<void*> __operand_stack;
std::vector<void*> __locals;


void store_local(int index, void* value)
{
    __locals[(__locals.size() - 1) - index] = value;
}


void* load_local(int index)
{
    return __locals[(__locals.size() - 1) - index];
}


void alloc_locals(int count)
{
    for (int i = 0; i < count; ++i) {
        __locals.push_back(nullptr);
    }
}


void free_locals(int count)
{
    for (int i = 0; i < count; ++i) {
        __locals.pop_back();
    }
}


void push_operand(void* value)
{
    __operand_stack.push_back(value);
}


void* get_operand(int offset)
{
    return __operand_stack[(__operand_stack.size() - 1) - offset];
}


void pop_operand()
{
    __operand_stack.pop_back();
}



struct Bytecode {
    enum : u8 {
        iconst_0     = 0x03,
        iconst_1     = 0x04,
        istore_1     = 0x3c,
        istore_2     = 0x3d,
        istore_3     = 0x3e,
        iload_1      = 0x1b,
        iinc         = 0x84,
        __goto       = 167,
        invokestatic = 184,
        vreturn      = 0xb1,
    };
};


struct ClassTableEntry {
    Slice name_;
    Class* class_;
};



std::vector<ClassTableEntry> class_table;



void execute_bytecode(Class* clz, const u8* bytecode);



void invoke_method(Class* clz, const ClassFile::MethodInfo* method)
{
    for (int i = 0; i < method->attributes_count_.get(); ++i) {
        auto attr =
            (ClassFile::AttributeInfo*)
            ((const char*)method + sizeof(ClassFile::MethodInfo));

        if (clz->load_string_constant(attr->attribute_name_index_.get()) ==
            Slice::from_c_str("Code")) {

            auto bytecode =
                ((const u8*)attr)
                + sizeof(ClassFile::AttributeCode);

            const auto local_count =
                std::max(((ClassFile::AttributeCode*)attr)->max_locals_.get(),
                         (u16)4);

            alloc_locals(local_count);

            execute_bytecode(clz, bytecode);

            free_locals(local_count);
        }
    }
}



Class* load_class(Class* current_module, u16 class_index)
{
    auto c_clz = (const ClassFile::ConstantClass*)
        current_module->load_constant(class_index);

    auto cname = current_module->load_string_constant(c_clz->name_index_.get());

    for (auto& entry : class_table) {
        if (entry.name_ == cname) {
            return entry.class_;
        }
    }

    return nullptr;
}



void invoke_static(Class* clz, u16 index)
{
    auto ref = (const ClassFile::ConstantRef*)clz->load_constant(index);

    Class* t_clz = load_class(clz, ref->class_index_.get());

    if (t_clz == nullptr) {
        puts("failed to load class, TODO: raise error...");
        while (true) ;
    }

    auto nt = (const ClassFile::ConstantNameAndType*)
        t_clz->load_constant(ref->name_and_type_index_.get());

    if (t_clz->methods_) {
        for (int i = 0; i < t_clz->method_count_; ++i) {
            if (t_clz->methods_[i]->name_index_.get() == nt->name_index_.get() and
                t_clz->methods_[i]->descriptor_index_.get() == nt->descriptor_index_.get()) {
                invoke_method(t_clz, t_clz->methods_[i]);
            }
        }
    }
}



void execute_bytecode(Class* clz, const u8* bytecode)
{
    u32 pc = 0;

    while (true) {
        switch (bytecode[pc]) {
        case Bytecode::iconst_0:
            push_operand((void*)(int)0);
            ++pc;
            break;

        case Bytecode::iconst_1:
            push_operand((void*)(int)1);
            ++pc;
            break;

        case Bytecode::istore_1:
            store_local(1, get_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::istore_2:
            store_local(2, get_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::istore_3:
            store_local(3, get_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::iload_1:
            push_operand(load_local(1));
            ++pc;
            break;

        case Bytecode::iinc:
            store_local(bytecode[pc + 1],
                        (void*)(intptr_t)
                        ((int)(intptr_t)(load_local(bytecode[pc + 1])) +
                         bytecode[pc + 2]));
            // printf("%d\n", (int)(intptr_t)load_local(1));
            pc += 3;
            break;

        case Bytecode::__goto:
            pc += ((network_s16*)(bytecode + pc + 1))->get();
            break;

        case Bytecode::vreturn:
            return;

        case Bytecode::invokestatic:
            ++pc;
            invoke_static(clz, ((network_u16*)(bytecode + pc))->get());
            pc += 2;
            break;

        default:
            printf("unrecognized bytecode instruction %d\n", bytecode[pc]);
            for (int i = 0; i < 10; ++i) {
                std::cout << (int)bytecode[i] << std::endl;
            }
            while (true) ;
        }
    }
}



Class* parse_classfile(Slice classname, const char* name)
{
    if (auto str = get_file_contents(name)) {
        auto h1 = reinterpret_cast<const ClassFile::HeaderSection1*>(str);

        if (h1->magic_.get() not_eq 0xcafebabe) {
            return nullptr;
        }

        auto clz = (Class*)malloc(sizeof(Class));

        if (not clz) {
            puts("failed to alloc constant class memory");
            while (true) ; // TODO: raise error
        }

        new (clz)Class();

        str += sizeof(ClassFile::HeaderSection1);

        clz->constants_ = (const ClassFile::ConstantHeader**)
            malloc(sizeof(ClassFile::ConstantHeader*)
                   * h1->constant_count_.get());

        if (not clz->constants_) {
            puts("failed to alloc constant pool");
            while (true) ; // TODO: raise error
        }

        for (int i = 0; i < h1->constant_count_.get() - 1; ++i) {
            clz->constants_[i] = (const ClassFile::ConstantHeader*)str;
            switch (static_cast<ClassFile::ConstantType>(*str)) {
            default:
                printf("error, constant %d\n", str[0]);
                while (true) ;
                break;

            case ClassFile::ConstantType::t_class:
                str += sizeof(ClassFile::ConstantClass);
                break;

            case ClassFile::ConstantType::t_field_ref:
                str += sizeof(ClassFile::ConstantRef);
                break;

            case ClassFile::ConstantType::t_method_ref:
                str += sizeof(ClassFile::ConstantRef);
                break;

            case ClassFile::ConstantType::t_interface_method_ref:
                str += sizeof(ClassFile::ConstantRef);
                break;

            case ClassFile::ConstantType::t_string:
                str += sizeof(ClassFile::ConstantString);
                break;

            case ClassFile::ConstantType::t_integer:
                str += sizeof(ClassFile::ConstantInteger);
                break;

            case ClassFile::ConstantType::t_float:
                str += sizeof(ClassFile::ConstantFloat);
                break;

            case ClassFile::ConstantType::t_long:
                str += sizeof(ClassFile::ConstantLong);
                break;

            case ClassFile::ConstantType::t_double:
                str += sizeof(ClassFile::ConstantDouble);
                break;

            case ClassFile::ConstantType::t_name_and_type:
                str += sizeof(ClassFile::ConstantNameAndType);
                break;

            case ClassFile::ConstantType::t_utf8:
                str += sizeof(ClassFile::ConstantUtf8)
                    + ((const ClassFile::ConstantUtf8*)str)->length_.get();
                break;

            case ClassFile::ConstantType::t_method_handle:
                str += sizeof(ClassFile::ConstantMethodHandle);
                break;

            case ClassFile::ConstantType::t_method_type:
                str += sizeof(ClassFile::ConstantMethodType);
                break;

            case ClassFile::ConstantType::t_invoke_dynamic:
                str += sizeof(ClassFile::ConstantInvokeDynamic);
                break;
            }
        }

        auto h2 = reinterpret_cast<const ClassFile::HeaderSection2*>(str);
        str += sizeof(ClassFile::HeaderSection2);

        if (h2->interfaces_count_.get()) {
            puts("TODO: load interfaces from classfile!");
            while (true) ;
        }

        auto h3 = reinterpret_cast<const ClassFile::HeaderSection3*>(str);
        str += sizeof(ClassFile::HeaderSection3);

        if (h3->fields_count_.get()) {
            printf("found %d fields!\n", h3->fields_count_.get());
            while (true) ;
        }

        auto h4 = reinterpret_cast<const ClassFile::HeaderSection4*>(str);
        str += sizeof(ClassFile::HeaderSection4);

        if (h4->methods_count_.get()) {

            clz->methods_ =
                (ClassFile::MethodInfo**)
                malloc(sizeof(ClassFile::MethodInfo*)
                       * h4->methods_count_.get());

            if (clz->methods_ == nullptr) {
                puts("failed to alloc method table");
                while (true) ; // TODO: raise error...
            }

            clz->method_count_ = h4->methods_count_.get();

            for (int i = 0; i < h4->methods_count_.get(); ++i) {
                auto method = (ClassFile::MethodInfo*)str;
                str += sizeof(ClassFile::MethodInfo*);

                clz->methods_[i] = method;

                for (int i = 0; i < method->attributes_count_.get(); ++i) {
                    auto attr = (ClassFile::AttributeInfo*)str;
                    str +=
                        sizeof(ClassFile::AttributeInfo) +
                        attr->attribute_length_.get();
                }
            }
        }


        auto h5 = reinterpret_cast<const ClassFile::HeaderSection5*>(str);
        str += sizeof(ClassFile::HeaderSection5);

        if (h5->attributes_count_.get()) {
            for (int i = 0; i < h5->attributes_count_.get(); ++i) {
                auto attr = (ClassFile::AttributeInfo*)str;
                str +=
                    sizeof(ClassFile::AttributeInfo) +
                    attr->attribute_length_.get();

                auto attr_name =
                    clz->load_string_constant(attr->attribute_name_index_.get());

                if (attr_name == Slice::from_c_str("SourceFile")) {
                    // TODO...
                } else {
                    // TOOD...
                }
            }
        }

        class_table.push_back({classname, clz});

        return clz;
    }

    return nullptr;
}



}



}





////////////////////////////////////////////////////////////////////////////////



int main()
{
    if (auto clz = java::jvm::parse_classfile(java::Slice::from_c_str("HelloWorldApp"),
                                              "HelloWorldApp.class")) {
        puts("parsed classfile header correctly");

        if (auto entry = clz->load_method("main")) {
            java::jvm::invoke_method(clz, entry);
        }

    } else {
        puts("failed to parse class file");
    }
}







#include <stdio.h>
#include <stdlib.h>


const char* get_file_contents(const char* name)
{
    char * buffer = 0;
    long length;
    FILE * f = fopen(name, "rb");

    if (f) {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = (char*)malloc (length);
        if (buffer) {
            fread(buffer, 1, length, f);
        }
        fclose (f);
    }

    return buffer;
}
