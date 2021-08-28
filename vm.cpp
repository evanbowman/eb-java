#include "classfile.hpp"
#include "class.hpp"
#include "endian.hpp"
#include "object.hpp"
#include <iostream>
#include <map>
#include <string.h>
#include <vector>



const char* get_file_contents(const char* file);



namespace java {
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


void* load_operand(int offset)
{
    return __operand_stack[(__operand_stack.size() - 1) - offset];
}


void pop_operand()
{
    __operand_stack.pop_back();
}



struct Bytecode {
    enum : u8 {
        nop           = 0x00,
        pop           = 0x57,
        new_inst      = 0xbb,
        dup           = 0x59,
        aload         = 0x19,
        aload_0       = 0x2a,
        aload_1       = 0x2b,
        aload_2       = 0x2c,
        aload_3       = 0x2d,
        astore        = 0x3a,
        astore_0      = 0x4b,
        astore_1      = 0x4c,
        astore_2      = 0x4d,
        astore_3      = 0x4e,
        areturn       = 0xb0,
        iconst_0      = 0x03,
        iconst_1      = 0x04,
        istore        = 0x36,
        istore_0      = 0x3b,
        istore_1      = 0x3c,
        istore_2      = 0x3d,
        istore_3      = 0x3e,
        iload         = 0x15,
        iload_0       = 0x1a,
        iload_1       = 0x1b,
        iload_2       = 0x1c,
        iload_3       = 0x1d,
        iadd          = 0x60,
        getfield      = 0xb4,
        putfield      = 0xb5,
        iinc          = 0x84,
        __goto        = 167,
        invokestatic  = 184,
        invokevirtual = 0xb6,
        invokespecial = 0xb7,
        vreturn       = 0xb1,
    };
};


struct ClassTableEntry {
    Slice name_;
    Class* class_;
};



std::vector<ClassTableEntry> class_table;



void execute_bytecode(Class* clz, const u8* bytecode);



void invoke_method(Class* clz,
                   Object* self,
                   const ClassFile::MethodInfo* method)
{
    for (int i = 0; i < method->attributes_count_.get(); ++i) {
        auto attr =
            (ClassFile::AttributeInfo*)
            ((const char*)method + sizeof(ClassFile::MethodInfo));

        if (clz->constants_->load_string(attr->attribute_name_index_.get()) ==
            Slice::from_c_str("Code")) {

            auto bytecode =
                ((const u8*)attr)
                + sizeof(ClassFile::AttributeCode);

            const auto local_count =
                std::max(((ClassFile::AttributeCode*)attr)->max_locals_.get(),
                         (u16)4); // Why a min of four? istore_0-3, so there
                                  // must be at least four slots.

            alloc_locals(local_count);
            store_local(0, self);

            execute_bytecode(clz, bytecode);

            free_locals(local_count);
        }
    }
}



Class* load_class(Class* current_module, u16 class_index)
{
    auto c_clz = (const ClassFile::ConstantClass*)
        current_module->constants_->load(class_index);

    auto cname = current_module->constants_->load_string(c_clz->name_index_.get());

    for (auto& entry : class_table) {
        if (entry.name_ == cname) {
            return entry.class_;
        }
    }

    return nullptr;
}



// I know, this is unfortunately quite slow. If we had tons of memory to work
// with, we'd create all sorts of fast dispatch tables and stuff, but this VM is
// intended to be used for a system with limited memory, so we just can't create
// sparse lookup tables for every class in a program.
const ClassFile::MethodInfo* lookup_method(Class* clz,
                                           Slice lhs_name,
                                           Slice lhs_type)
{
    if (clz->methods_) {
        for (int i = 0; i < clz->method_count_; ++i) {
            u16 name_index = clz->methods_[i]->name_index_.get();
            u16 type_index = clz->methods_[i]->descriptor_index_.get();

            auto rhs_name = clz->constants_->load_string(name_index);
            auto rhs_type = clz->constants_->load_string(type_index);

            if (lhs_type == rhs_type and lhs_name == rhs_name) {
                return clz->methods_[i];
            }
        }
    }

    return nullptr;
}



void dispatch_method(Class* clz, Object* self, u16 method_index)
{
    auto ref = (const ClassFile::ConstantRef*)clz->constants_->load(method_index);

    Class* t_clz = load_class(clz, ref->class_index_.get());
    if (t_clz == nullptr) {
        printf("failed to load class %d, TODO: raise error...\n",
               ref->class_index_.get());
        while (true) ;
    }

    auto nt = (const ClassFile::ConstantNameAndType*)
        clz->constants_->load(ref->name_and_type_index_.get());

    auto lhs_name = clz->constants_->load_string(nt->name_index_.get());
    auto lhs_type = clz->constants_->load_string(nt->descriptor_index_.get());

    if (auto mtd = lookup_method(t_clz, lhs_name, lhs_type)) {
        invoke_method(t_clz, self, mtd);
    } else {
        // TODO: raise error
        puts("method lookup failed!");
        while (true);
    }
}



void invoke_special(Class* clz, u16 method_index)
{
    puts("invoke_special");

    auto self = (Object*)load_operand(0);
    pop_operand();

    dispatch_method(clz, self, method_index);
}



void* jvm_malloc(size_t size)
{
    static size_t total = 0;
    total += size;
    printf("vm malloc %zu (total %zu)\n", size, total);
    return malloc(size);
}



Object* make_instance(Class* clz, u16 class_constant)
{
    auto t_clz = load_class(clz, class_constant);

    if (t_clz) {
        auto sub = (SubstitutionField*)clz->constants_->load(clz->cpool_highest_field_);

        printf("highest field offset: %d\n", sub->offset_);
        printf("instance size %ld\n", sizeof(Object) + sub->offset_ + (1 << sub->size_));

        auto mem = (Object*)jvm_malloc(sizeof(Object) + sub->offset_ + (1 << sub->size_));
        new (mem) Object();
        mem->class_ = t_clz;
        return mem;
    }

    // TODO: fatal error...
    puts("warning! failed to alloc class!");
    return nullptr;
}



void execute_bytecode(Class* clz, const u8* bytecode)
{
    u32 pc = 0;

    while (true) {
        switch (bytecode[pc]) {
        case Bytecode::nop:
            ++pc;
            break;

        case Bytecode::pop:
            pop_operand();
            ++pc;
            break;

        case Bytecode::new_inst:
            push_operand(make_instance(clz, ((network_u16*)&bytecode[pc + 1])->get()));
            // printf("new %p\n", load_operand(0));
            pc += 3;
            break;

        case Bytecode::areturn:
            // NOTE: we implement return values on the stack. nothing to do here.
            return;

        case Bytecode::dup:
            // printf("dup %p\n", load_operand(0));
            push_operand(load_operand(0));
            ++pc;
            break;

        case Bytecode::iconst_0:
            push_operand((void*)(int)0);
            ++pc;
            break;

        case Bytecode::iconst_1:
            push_operand((void*)(int)1);
            ++pc;
            break;

        case Bytecode::getfield: {
            // printf("%p\n", load_operand(0));
            auto arg = (Object*)load_operand(0);
            pop_operand();
            push_operand(arg->get_field(((network_u16*)&bytecode[pc + 1])->get()));
            pc += 3;
            break;
        }

        case Bytecode::putfield:
            ((Object*)load_operand(1))->put_field(((network_u16*)&bytecode[pc + 1])->get(),
                                                  load_operand(0));
            pop_operand();
            pop_operand();
            pc += 3;
            // while (true) ;
            break;

        case Bytecode::iadd: {
            const int result =
                (int)(intptr_t)load_operand(0) +
                (int)(intptr_t)load_operand(1);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::astore:
        case Bytecode::istore:
            store_local(bytecode[pc + 1], load_operand(0));
            pop_operand();
            pc += 2;
            break;

        case Bytecode::astore_0:
        case Bytecode::istore_0:
            store_local(0, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_1:
        case Bytecode::istore_1:
            store_local(1, load_operand(0));
            // printf("store local 1 %p\n", load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_2:
        case Bytecode::istore_2:
            store_local(2, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_3:
        case Bytecode::istore_3:
            store_local(3, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::aload:
        case Bytecode::iload:
            push_operand(load_local(bytecode[pc + 1]));
            pc += 2;
            break;

        case Bytecode::aload_0:
        case Bytecode::iload_0:
            push_operand(load_local(0));
            // printf("load0 %p\n", load_local(0));
            ++pc;
            break;

        case Bytecode::aload_1:
        case Bytecode::iload_1:
            push_operand(load_local(1));
            // printf("load1 %p\n", load_local(1));
            ++pc;
            break;

        case Bytecode::aload_2:
        case Bytecode::iload_2:
            push_operand(load_local(2));
            ++pc;
            break;

        case Bytecode::aload_3:
        case Bytecode::iload_3:
            push_operand(load_local(3));
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
            dispatch_method(clz,
                            nullptr,
                            ((network_u16*)(bytecode + pc))->get());
            pc += 2;
            break;

        case Bytecode::invokevirtual: {
            ++pc;
            auto obj = (Object*)load_operand(0);
            pop_operand();
            dispatch_method(clz,
                            obj,
                            ((network_u16*)(bytecode + pc))->get());
            pc += 2;
            break;
        }

        case Bytecode::invokespecial:
            ++pc;
            invoke_special(clz, ((network_u16*)(bytecode + pc))->get());
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



const char* parse_classfile_fields(const char* str, Class* clz, int constant_count)
{
    // Here, we're loading the class file, and we want to determine all of the
    // correct byte offsets of all of the class instance's fields.

    auto h3 = reinterpret_cast<const ClassFile::HeaderSection3*>(str);
    str += sizeof(ClassFile::HeaderSection3);

    if (auto nfields = h3->fields_count_.get()) {

        auto fields = (SubstitutionField*)jvm_malloc(sizeof(SubstitutionField) * nfields);

        u16 offset = 0;

        clz->constants_->reserve_fields(nfields);

        for (int i = 0; i < nfields; ++i) {
            auto field = (const ClassFile::FieldInfo*)str;
            str += sizeof(ClassFile::FieldInfo);

            printf("field %d has offset %d\n", i, offset);

            fields[i].offset_ = offset;

            // printf("field desc %d\n", );

            SubstitutionField::Size field_size = SubstitutionField::b4;


            auto field_type =
                clz->constants_->load_string(field->descriptor_index_.get());

            auto field_name =
                clz->constants_->load_string(field->name_index_.get());


            if (field_type == Slice::from_c_str("I")) {
                field_size = SubstitutionField::b4;
            } else if (field_type == Slice::from_c_str("S")) {
                field_size = SubstitutionField::b2;
            } else {
                std::cout << std::string(field_type.ptr_, field_type.length_) << std::endl;
                puts("TODO: field sizes...");
                while (true) ;
            }

            fields[i].size_ = field_size;

            for (int j = 0; j < constant_count - 1; ++j) {
                int ind = j + 1;

                auto c = clz->constants_->load(ind);
                if (c->tag_ == ClassFile::ConstantType::t_field_ref) {
                    auto ref = ((ClassFile::ConstantRef*)c);
                    auto nt = (ClassFile::ConstantNameAndType*)
                        clz->constants_->load(ref->name_and_type_index_.get());

                    // FIXME: we also need to verify that the class type matches...
                    if (clz->constants_->load_string(nt->name_index_.get()) == field_name and
                        clz->constants_->load_string(nt->descriptor_index_.get()) == field_type) {
                        printf("found field at %d\n", ind);

                        clz->constants_->bind_field(ind, &fields[i]);
                        clz->cpool_highest_field_ = ind;
                    }

                }
            }


            offset += (1 << field_size);


            for (int i = 0; i < field->attributes_count_.get(); ++i) {
                auto attr = (ClassFile::AttributeInfo*)str;
                str +=
                    sizeof(ClassFile::AttributeInfo) +
                    attr->attribute_length_.get();
            }
        }
    }

    return str;
}



Class* parse_classfile(Slice classname, const char* name)
{
    if (auto str = get_file_contents(name)) {
        auto h1 = reinterpret_cast<const ClassFile::HeaderSection1*>(str);

        if (h1->magic_.get() not_eq 0xcafebabe) {
            return nullptr;
        }

        auto clz = (Class*)jvm_malloc(sizeof(Class));

        if (not clz) {
            puts("failed to alloc constant class memory");
            while (true) ; // TODO: raise error
        }

        new (clz)Class();

        str += sizeof(ClassFile::HeaderSection1);


        clz->constants_ = new ConstantPoolArrayImpl();
        str = clz->constants_->parse(*h1);


        auto h2 = reinterpret_cast<const ClassFile::HeaderSection2*>(str);
        str += sizeof(ClassFile::HeaderSection2);


        clz->super_ = load_class(clz, h2->super_class_.get());


        if (h2->interfaces_count_.get()) {
            puts("TODO: load interfaces from classfile!");
            while (true) ;
        }

        str = parse_classfile_fields(str, clz, h1->constant_count_.get());

        auto h4 = reinterpret_cast<const ClassFile::HeaderSection4*>(str);
        str += sizeof(ClassFile::HeaderSection4);

        if (h4->methods_count_.get()) {

            clz->methods_ =
                (const ClassFile::MethodInfo**)
                jvm_malloc(sizeof(ClassFile::MethodInfo*)
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
                    clz->constants_->load_string(attr->attribute_name_index_.get());

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



void bootstrap()
{
    // NOTE: I manually edited the bytecode in the Object classfile, which is
    // why I do not provide the source code. It's hand-rolled java bytecode.
    if (parse_classfile(Slice::from_c_str("java/lang/Object"),
                        "Object.class")) {
        puts("successfully loaded Object root!");
    }
}



}



}





////////////////////////////////////////////////////////////////////////////////



int main()
{
    java::jvm::bootstrap();

    if (auto clz = java::jvm::parse_classfile(java::Slice::from_c_str("HelloWorldApp"),
                                              "HelloWorldApp.class")) {
        puts("parsed classfile header correctly");

        if (auto entry = clz->load_method("main")) {
            java::jvm::invoke_method(clz, nullptr, entry);
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
