#include "classfile.hpp"
#include "class.hpp"
#include "endian.hpp"
#include "object.hpp"
#include <iostream>
#include <string.h>
#include <vector>
#include "array.hpp"
#include "jar.hpp"



namespace java {
namespace jvm {



static const char* jar_file_data;



static Class primitive_array_class {
    nullptr, // constants
    nullptr, // methods
    nullptr, // super (will be filled in with Object by the bootstrap code)
    nullptr, // classfile data
    -1,      // cpool_highest_field
    0,       // method count
};


// We make the distinction between primitive arrays and reference arrays,
// because later on, we will need to track gc roots.
static Class reference_array_class {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    -1,
    0
};


// TODO: maintain a secondary bitvector, where we can keep track of whether an
// operand stack element is primitive or instance.
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


void push_operand(float* value)
{
    push_operand((void*)(intptr_t)*(int*)value);
}


void* load_operand(int offset)
{
    return __operand_stack[(__operand_stack.size() - 1) - offset];
}


float __load_operand_f_impl(void** val)
{
    return *(float*)val;
}


float load_operand_f(int offset)
{
    auto val = load_operand(offset);
    return __load_operand_f_impl(&val);
}


s32 load_operand_i(int offset)
{
    return (s32)(intptr_t)load_operand(offset);
}


void pop_operand()
{
    __operand_stack.pop_back();
}



struct Bytecode {
    enum : u8 {
        nop             = 0x00,
        pop             = 0x57,
        swap            = 0x5f,
        ldc             = 0x12,
        new_inst        = 0xbb,
        dup             = 0x59,
        // dup_x1        = 0x5a, // TODO
        // dup_x2        = 0x5b, // TODO
        dup2            = 0x5c,
        // dup2_x1       = 0x5d, // TODO
        // dup2_x2       = 0x5e, // TODO
        bastore         = 0x54,
        baload          = 0x33,
        bipush          = 0x10,
        castore         = 0x55,
        caload          = 0x34,
        newarray        = 0xbc,
        arraylength     = 0xbe,
        aaload          = 0x32,
        aastore         = 0x53,
        aload           = 0x19,
        aload_0         = 0x2a,
        aload_1         = 0x2b,
        aload_2         = 0x2c,
        aload_3         = 0x2d,
        astore          = 0x3a,
        astore_0        = 0x4b,
        astore_1        = 0x4c,
        astore_2        = 0x4d,
        astore_3        = 0x4e,
        anewarray       = 0xbd,
        areturn         = 0xb0,
        aconst_null     = 0x01,
        checkcast       = 0xc0,
        instanceof      = 0xc1,
        iconst_m1       = 0x02,
        iconst_0        = 0x03,
        iconst_1        = 0x04,
        iconst_2        = 0x05,
        iconst_3        = 0x06,
        iconst_4        = 0x07,
        iconst_5        = 0x08,
        istore          = 0x36,
        istore_0        = 0x3b,
        istore_1        = 0x3c,
        istore_2        = 0x3d,
        istore_3        = 0x3e,
        iload           = 0x15,
        iload_0         = 0x1a,
        iload_1         = 0x1b,
        iload_2         = 0x1c,
        iload_3         = 0x1d,
        iadd            = 0x60,
        isub            = 0x64,
        idiv            = 0x6c,
        imul            = 0x68,
        ineg            = 0x74,
        i2f             = 0x86,
        i2c             = 0x92,
        i2s             = 0x93,
        iinc            = 0x84,
        iastore         = 0x4f,
        iaload          = 0x2e,
        if_acmpeq       = 0xa5,
        if_acmpne       = 0xa6,
        if_icmpeq       = 0x9f,
        if_icmpne       = 0xa0,
        if_icmplt       = 0xa1,
        if_icmpge       = 0xa2,
        if_icmpgt       = 0xa3,
        if_icmple       = 0xa4,
        if_eq           = 0x99,
        if_ne           = 0x9a,
        if_lt           = 0x9b,
        if_ge           = 0x9c,
        if_gt           = 0x9d,
        if_le           = 0x9e,
        if_nonnull      = 0xc7,
        if_null         = 0xc6,
        fconst_0        = 0x0b,
        fconst_1        = 0x0c,
        fconst_2        = 0x0d,
        fadd            = 0x62,
        fdiv            = 0x6e,
        fmul            = 0x6a,
        fload           = 0x17,
        fload_0         = 0x22,
        fload_1         = 0x23,
        fload_2         = 0x24,
        fload_3         = 0x25,
        fstore          = 0x38,
        fstore_0        = 0x43,
        fstore_1        = 0x44,
        fstore_2        = 0x45,
        fstore_3        = 0x46,
        faload          = 0x30,
        fastore         = 0x51,
        fcmpl           = 0x95,
        fcmpg           = 0x96,
        freturn         = 0xae,
        saload          = 0x35,
        sastore         = 0x56,
        sipush          = 0x11,
        getfield        = 0xb4,
        putfield        = 0xb5,
        __goto          = 0xa7,
        __goto_w        = 0xc8,
        invokestatic    = 0xb8,
        invokevirtual   = 0xb6,
        invokespecial   = 0xb7,
        invokeinterface = 0xb9,
        vreturn         = 0xb1,
        // jsr           = 0xa8, unimplemented
        // jsr_w         = 0xc9,
    };
};


struct ClassTableEntry {
    Slice name_;
    Class* class_;
};



static std::vector<ClassTableEntry> class_table;



Class* import(Slice classpath)
{
    puts("loading class from jar!");

    auto data = jar::load_classfile(jar_file_data, classpath);
    if (data.length_) {
        puts("loaded classfile from jar!");

        if (auto clz = parse_classfile(classpath, data.ptr_)) {
            puts("parsed classfile correctly!");
            return clz;
        }
    }
    return nullptr;
}



void register_class(Slice name, Class* clz)
{
    class_table.push_back({name, clz});
}



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

    return import(cname);
    // std::cout << "missing " << std::string(cname.ptr_, cname.length_) << std::endl;
    // while( true);
    // // Ok, if we haven't found the class yet, we should go ahead and dynamically
    // // load it!
    // // TODO: load class from jar

    // return nullptr;
}



std::pair<const ClassFile::MethodInfo*, Class*> lookup_method(Class* clz,
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
                return {clz->methods_[i], clz};
            }
        }
    }

    if (clz->super_ == nullptr) {
        return {nullptr, clz};
    } else {
        return lookup_method(clz->super_, lhs_name, lhs_type);
    }
}



static void dispatch_method(Class* clz,
                            Object* self,
                            u16 method_index,
                            bool direct_dispatch)
{
    auto ref = (const ClassFile::ConstantRef*)
        clz->constants_->load(method_index);

    auto nt = (const ClassFile::ConstantNameAndType*)
        clz->constants_->load(ref->name_and_type_index_.get());

    auto lhs_name = clz->constants_->load_string(nt->name_index_.get());
    auto lhs_type = clz->constants_->load_string(nt->descriptor_index_.get());

    auto mtd = [&] {
        if (self and not direct_dispatch) {
            return lookup_method(self->class_, lhs_name, lhs_type);
        } else {
            // If we do not have a self pointer we're looking up a static
            // method, if direct_dispatch, we're processing an invoke_special
            // instruction.
            Class* t_clz = load_class(clz, ref->class_index_.get());
            if (t_clz == nullptr) {
                printf("failed to load class %d, TODO: raise error...\n",
                       ref->class_index_.get());
                while (true) ;
            }
            return lookup_method(t_clz, lhs_name, lhs_type);
        }
    }();

    if (mtd.first) {
        invoke_method(mtd.second, self, mtd.first);
    } else {
        // TODO: raise error
        puts("method lookup failed!");
        while (true);
    }
}



void invoke_special(Class* clz, u16 method_index)
{
    auto self = (Object*)load_operand(0);
    pop_operand();

    dispatch_method(clz, self, method_index, true);
}



void* malloc(size_t size)
{
    static size_t total = 0;
    total += size;
    printf("vm malloc %zu (total %zu)\n", size, total);
    return ::malloc(size);
}



Object* make_instance(Class* clz, u16 class_constant)
{
    auto t_clz = load_class(clz, class_constant);

    if (t_clz) {

        const auto fields_size = clz->instance_fields_size();

        printf("instance size %ld\n", fields_size);

        auto mem = (Object*)jvm::malloc(sizeof(Object) + fields_size);
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

        case Bytecode::swap: {
            auto one = load_operand(0);
            auto two = load_operand(1);
            pop_operand();
            pop_operand();
            push_operand(one);
            push_operand(two);
            ++pc;
            break;
        }

        case Bytecode::ldc: {
            auto c = clz->constants_->load(bytecode[pc + 1]);
            switch (c->tag_) {
            case ClassFile::ConstantType::t_float: {
                auto cfl = (ClassFile::ConstantFloat*)c;
                float result;
                auto input = cfl->value_.get();
                memcpy(&result, &input, sizeof(float));
                push_operand(&result);
                break;
            }

            case ClassFile::ConstantType::t_integer: {
                auto cint = (ClassFile::ConstantInteger*)c;
                push_operand((void*)(intptr_t)cint->value_.get());
                break;
            }

            default:
                puts("unhandled ldc...");
                while (true) ;
                break;
            }
            pc += 2;
            break;
        }

        case Bytecode::new_inst:
            push_operand(make_instance(clz, ((network_u16*)&bytecode[pc + 1])->get()));
            // printf("new %p\n", load_operand(0));
            pc += 3;
            break;

        case Bytecode::bipush:
            push_operand((void*)(intptr_t)(s32)bytecode[pc + 1]);
            pc += 2;
            break;

        case Bytecode::anewarray: {
            auto array = Array::create(load_operand_i(0), sizeof(Object*));
            pop_operand();
            array->object_.class_ = &reference_array_class;
            push_operand(array);
            pc += 3;
            break;
        }

        case Bytecode::newarray: {
            int element_size = 4;
            switch (bytecode[pc + 1]) {
            case 4:
            case 5:
            case 8:
                element_size = 1;
                break;

            case 6:
            case 10:
                element_size = 4;
                break;

            case 9:
                element_size = 2;
                break;

            case 7:
            case 11:
                element_size = 8;
                break;
            }

            const int element_count = load_operand_i(0);
            pop_operand();

            auto array = Array::create(element_count, element_size);

            array->object_.class_ = &primitive_array_class;
            push_operand(array);
            pc += 2;
            break;
        }

        case Bytecode::arraylength: {
            auto array = (Array*)load_operand(0);
            pop_operand();
            push_operand((void*)(intptr_t)array->size_);
            ++pc;
            break;
        }

        case Bytecode::aaload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);

            static_assert(sizeof(s32) == sizeof(float),
                          "This code assumes that both int "
                          "and float are four bytes");

            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                Object* result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand(result);
            } else {
                puts("array index out of bounds");
                while (true) ;
            }
            ++pc;
            break;
        }

        case Bytecode::aastore: {
            auto array = (Array*)load_operand(2);
            auto value = (Object*)load_operand(0);
            s32 index = load_operand_i(1);

            static_assert(sizeof(s32) == sizeof(float),
                          "This code assumes that both int "
                          "and float are four bytes");

            pop_operand();
            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                // TODO: throw error
                puts("array index out of bounds!");
                while (true) ;
            }
            ++pc;
            break;
        }

        case Bytecode::aconst_null:
            push_operand((void*)nullptr);
            pc += 1;
            break;

        case Bytecode::checkcast: {
            auto other =
                load_class(clz, ((network_u16*)&bytecode[pc + 1])->get());
            auto obj = (Object*)load_operand(0);
            pop_operand();

            auto current = obj->class_;
            while (current) {
                // FIXME: check for successful cast to implemented interfaces
                if (current == other) {
                    goto CAST_SUCCESS;
                }
                current = current->super_;
            }

            // TODO: throw class cast exception!
            puts("bad cast");
            while (true) ;

            CAST_SUCCESS:
            pc += 3;
            break;
        }

        case Bytecode::instanceof: {
            auto other =
                load_class(clz, ((network_u16*)&bytecode[pc + 1])->get());
            auto obj = (Object*)load_operand(0);
            pop_operand();
            pc += 3;

            auto current = obj->class_;
            while (current) {
                // FIXME: check for successful cast to implemented interfaces
                if (current == other) {
                    goto FOUND_INSTANCEOF;
                }
                current = current->super_;
            }

            push_operand((void*)(intptr_t)0);
            break;

            FOUND_INSTANCEOF:
            push_operand((void*)(intptr_t)1);
            break;
        }

        case Bytecode::dup:
            push_operand(load_operand(0));
            ++pc;
            break;

        case Bytecode::dup2:
            push_operand(load_operand(1));
            push_operand(load_operand(1));
            ++pc;
            break;

        case Bytecode::iconst_m1:
            push_operand((void*)-1);
            ++pc;
            break;

        case Bytecode::iconst_0:
            push_operand((void*)(s32)0);
            ++pc;
            break;

        case Bytecode::iconst_1:
            push_operand((void*)(s32)1);
            ++pc;
            break;

        case Bytecode::iconst_2:
            push_operand((void*)(s32)2);
            ++pc;
            break;

        case Bytecode::iconst_3:
            push_operand((void*)(s32)3);
            ++pc;
            break;

        case Bytecode::iconst_4:
            push_operand((void*)(s32)4);
            ++pc;
            break;

        case Bytecode::iconst_5:
            push_operand((void*)(s32)5);
            ++pc;
            break;

        case Bytecode::getfield: {
            auto arg = (Object*)load_operand(0);
            pop_operand();
            push_operand(arg->get_field(((network_u16*)&bytecode[pc + 1])->get()));
            // printf("%d\n", (int)(intptr_t)load_operand(0));
            pc += 3;
            break;
        }

        case Bytecode::putfield:
            ((Object*)load_operand(1))->put_field(((network_u16*)&bytecode[pc + 1])->get(),
                                                  load_operand(0));
            pop_operand();
            pop_operand();
            pc += 3;
            break;

        case Bytecode::isub: {
            const s32 result = load_operand_i(1) - load_operand_i(0);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::iadd: {
            const s32 result = load_operand_i(0) + load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::idiv: {
            const s32 result = load_operand_i(1) / load_operand_i(0);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::imul: {
            const s32 result = load_operand_i(1) * load_operand_i(0);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::ineg: {
            const s32 result = -load_operand_i(0);
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::i2c: {
            u8 val = load_operand_i(0);
            pop_operand();
            push_operand((void*)(intptr_t)val);
            ++pc;
            break;
        }

        case Bytecode::i2f: {
            float val = load_operand_i(0);
            pop_operand();
            push_operand(&val);
            ++pc;
            break;
        }

        case Bytecode::i2s: {
            short val = load_operand_i(0);
            pop_operand();
            push_operand((void*)(intptr_t)val);
            ++pc;
            break;
        }

        case Bytecode::if_acmpeq:
            if (load_operand(0) == load_operand(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_acmpne:
            if (load_operand(0) not_eq load_operand(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmpeq:
            if (load_operand_i(0) == load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmpne:
            if (load_operand_i(0) not_eq load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmplt:
            if (load_operand_i(0) < load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmpge:
            if (load_operand_i(0) > load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmple:
            if (load_operand_i(0) <= load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_eq:
            if (load_operand_i(0) == 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_ne:
            if (load_operand_i(0) not_eq 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_lt:
            if (load_operand_i(0) < 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_ge:
            if (load_operand_i(0) >= 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_gt:
            if (load_operand_i(0) > 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_le:
            if (load_operand_i(0) < 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_nonnull:
            if (load_operand(0) not_eq nullptr) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_null:
            if (load_operand(0) == nullptr) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::fconst_0: {
            static_assert(sizeof(float) == sizeof(s32) and
                          sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 0.f;
            push_operand(&f);
            ++pc;
            break;
        }

        case Bytecode::fconst_1: {
            static_assert(sizeof(float) == sizeof(s32) and
                          sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 1.f;
            push_operand(&f);
            ++pc;
            break;
        }

        case Bytecode::fconst_2: {
            static_assert(sizeof(float) == sizeof(s32) and
                          sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 2.f;
            push_operand(&f);
            ++pc;
            break;
        }

        case Bytecode::fadd: {
            float lhs = load_operand_f(0);
            float rhs = load_operand_f(1);
            pop_operand();
            pop_operand();
            auto result = lhs + rhs;
            push_operand(&result);
            ++pc;
            break;
        }

        case Bytecode::fdiv: {
            float lhs = load_operand_f(1);
            float rhs = load_operand_f(0);
            pop_operand();
            pop_operand();
            auto result = lhs / rhs;
            push_operand(&result);
            ++pc;
            break;
        }

        case Bytecode::fmul: {
            float lhs = load_operand_f(0);
            float rhs = load_operand_f(1);
            pop_operand();
            pop_operand();
            auto result = lhs * rhs;
            push_operand(&result);
            ++pc;
            break;
        }

        case Bytecode::fcmpg:
        case Bytecode::fcmpl: {
            auto rhs = load_operand_f(0);
            auto lhs = load_operand_f(1);

            pop_operand();
            pop_operand();

            if (lhs > rhs) {
                push_operand((void*)(intptr_t)1);
            } else if (lhs == rhs) {
                push_operand((void*)(intptr_t)0);
            } else if (lhs < rhs) {
                push_operand((void*)(intptr_t)-1);
            } else {
                if (bytecode[pc] == Bytecode::fcmpg) {
                    push_operand((void*)(intptr_t)1);
                } else {
                    push_operand((void*)(intptr_t)-1);
                }
            }
            ++pc;
            break;
        }

        case Bytecode::saload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);

            static_assert(sizeof(s32) == sizeof(float),
                          "This code assumes that both int "
                          "and float are four bytes");

            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                s16 result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand((void*)(intptr_t)result);
            } else {
                puts("array index out of bounds");
                while (true) ;
            }
            ++pc;
            break;
        }

        case Bytecode::sastore: {
            auto array = (Array*)load_operand(2);
            s16 value = load_operand_i(0);
            s32 index = load_operand_i(1);

            static_assert(sizeof(s32) == sizeof(float),
                          "This code assumes that both int "
                          "and float are four bytes");

            pop_operand();
            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                // TODO: throw error
                puts("array index out of bounds!");
                while (true) ;
            }
            ++pc;
            break;
        }

        case Bytecode::sipush: {
            s16 val = ((network_s16*)(bytecode + pc + 1))->get();
            push_operand((void*)(intptr_t)val);
            pc += 3;
            break;
        }

        case Bytecode::fstore:
        case Bytecode::astore:
        case Bytecode::istore:
            store_local(bytecode[pc + 1], load_operand(0));
            pop_operand();
            pc += 2;
            break;

        case Bytecode::fstore_0:
        case Bytecode::astore_0:
        case Bytecode::istore_0:
            store_local(0, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_1:
        case Bytecode::astore_1:
        case Bytecode::istore_1:
            store_local(1, load_operand(0));
            // printf("store local 1 %p\n", load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_2:
        case Bytecode::astore_2:
        case Bytecode::istore_2:
            store_local(2, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_3:
        case Bytecode::astore_3:
        case Bytecode::istore_3:
            store_local(3, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::fload:
        case Bytecode::aload:
        case Bytecode::iload:
            push_operand(load_local(bytecode[pc + 1]));
            pc += 2;
            break;

        case Bytecode::fload_0:
        case Bytecode::aload_0:
        case Bytecode::iload_0:
            push_operand(load_local(0));
            // printf("load0 %p\n", load_local(0));
            ++pc;
            break;

        case Bytecode::fload_1:
        case Bytecode::aload_1:
        case Bytecode::iload_1:
            push_operand(load_local(1));
            // printf("load1 %p\n", load_local(1));
            ++pc;
            break;

        case Bytecode::fload_2:
        case Bytecode::aload_2:
        case Bytecode::iload_2:
            push_operand(load_local(2));
            ++pc;
            break;

        case Bytecode::fload_3:
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

        case Bytecode::castore:
        case Bytecode::bastore: {
            auto array = (Array*)load_operand(2);
            u8 value = load_operand_i(0);
            s32 index = load_operand_i(1);

            pop_operand();
            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                *array->address(index) = value;
            } else {
                puts("array index out of bounds");
                while (true) ;
            }
            ++pc;
            break;
        }

        case Bytecode::caload:
        case Bytecode::baload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);

            if (array->check_bounds(index)) {
                u8 value = *array->address(index);
                push_operand((void*)(intptr_t)value);
            } else {
                puts("array index out of bounds");
                while (true) ;
            }
            break;
        }

        case Bytecode::fastore:
        case Bytecode::iastore: {
            auto array = (Array*)load_operand(2);
            s32 value = load_operand_i(0);
            s32 index = load_operand_i(1);

            static_assert(sizeof(s32) == sizeof(float),
                          "This code assumes that both int "
                          "and float are four bytes");

            pop_operand();
            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                // TODO: throw error
                puts("array index out of bounds!");
                while (true) ;
            }
            ++pc;
            break;
        }

        case Bytecode::faload:
        case Bytecode::iaload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);

            static_assert(sizeof(s32) == sizeof(float),
                          "This code assumes that both int "
                          "and float are four bytes");

            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                s32 result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand((void*)(intptr_t)result);
            } else {
                puts("array index out of bounds");
                while (true) ;
            }
            ++pc;
            break;
        }

        case Bytecode::__goto:
            pc += ((network_s16*)(bytecode + pc + 1))->get();
            break;

        case Bytecode::__goto_w:
            pc += ((network_s32*)(bytecode + pc + 1))->get();
            break;

        case Bytecode::areturn:
        case Bytecode::freturn:
        case Bytecode::vreturn:
            // NOTE: we implement return values on the stack. nothing to do here.
            return;

        case Bytecode::invokestatic:
            ++pc;
            dispatch_method(clz,
                            nullptr,
                            ((network_u16*)(bytecode + pc))->get(), false);
            pc += 2;
            break;

        case Bytecode::invokevirtual: {
            ++pc;
            auto obj = (Object*)load_operand(0);
            pop_operand();
            dispatch_method(clz,
                            obj,
                            ((network_u16*)(bytecode + pc))->get(), false);
            pc += 2;
            break;
        }

        case Bytecode::invokeinterface: {
            ++pc;
            auto obj = (Object*)load_operand(0);
            pop_operand();
            dispatch_method(clz,
                            obj,
                            ((network_u16*)(bytecode + pc))->get(), false);
            pc += 4;
            break;
        }

        case Bytecode::invokespecial:
            ++pc;
            invoke_special(clz, ((network_u16*)(bytecode + pc))->get());
            pc += 2;
            break;

        default:
            printf("unrecognized bytecode instruction %#02x\n", bytecode[pc]);
            for (int i = 0; i < 10; ++i) {
                std::cout << (int)bytecode[i] << std::endl;
            }
            while (true) ;
        }
    }
}




const char* get_file_contents(const char* name)
{
    char * buffer = 0;
    long length;
    FILE * f = fopen(name, "rb");

    if (f) {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = (char*)::malloc (length);
        if (buffer) {
            fread(buffer, 1, length, f);
        }
        fclose (f);
    }

    return buffer;
}



void bootstrap()
{
    // NOTE: I manually edited the bytecode in the Object classfile, which is
    // why I do not provide the source code. It's hand-rolled java bytecode.
    if (auto obj_class = parse_classfile(Slice::from_c_str("java/lang/Object"),
                                         get_file_contents("Object.class"))) {
        puts("successfully loaded Object root!");

        obj_class->super_ = nullptr;

        primitive_array_class.super_ = obj_class;
        reference_array_class.super_ = obj_class;
    }
}



void start(const char* jar_file_bytes)
{
    jar_file_data = jar_file_bytes;

    bootstrap();

    // jar::load_classfile(jar_file_bytes, "HelloWorldApp");
}



}



}





////////////////////////////////////////////////////////////////////////////////


#include <string>
#include <fstream>
#include <streambuf>

int main()
{
    std::ifstream t("TEST2.jar");
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());

    java::jvm::jar_file_data = str.c_str();


    java::jvm::bootstrap();

    // if (java::parse_classfile(java::Slice::from_c_str("test/Example"),
    //                           java::jvm::get_file_contents("Example.class"))) {
    //     puts("parsed base class");
    // }

    java::jvm::import(java::Slice::from_c_str("test/Example"));

    if (auto clz = java::jvm::import(java::Slice::from_c_str("test/HelloWorldApp"))) {
        puts("import main class success!");

        if (auto entry = clz->load_method("main")) {
            java::jvm::invoke_method(clz, nullptr, entry);
        }
    } else {
        puts("failed to import main class");
    }

    // if (auto clz = java::parse_classfile(java::Slice::from_c_str("test/HelloWorldApp"),
    //                                      java::jvm::get_file_contents("HelloWorldApp.class"))) {
    //     puts("parsed classfile header correctly");



    // } else {
    //     puts("failed to parse class file");
    // }
}
