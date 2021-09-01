#include "array.hpp"
#include "buffer.hpp"
#include "class.hpp"
#include "classfile.hpp"
#include "endian.hpp"
#include "jar.hpp"
#include "jni.hpp"
#include "object.hpp"
#include <iostream>
#include <string.h>
#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include "incbin.h"
#include "memory.hpp"
#include <math.h>
#include <chrono>



namespace java {
namespace jvm {



static const char* jar_file_data;



static Class primitive_array_class;



// We make the distinction between primitive arrays and reference arrays,
// because later on, we will need to track gc roots.
static Class reference_array_class;



static Class return_address_class;



// Our JVM implementation assumes that all values used with aload are instances
// of classes. Therefore, the jvm returnAddress datatype must be implemented as
// an instance of a pseudo-class. Newer compilers don't generate jsr/jsr_w/ret
// anyway, so this is only relevant when running legacy jars.
struct ReturnAddress {
    Object object_;
    u32 pc_;
};



ReturnAddress* make_return_address(u32 pc)
{
    auto mem = (ReturnAddress*)heap::allocate(sizeof(ReturnAddress));
    if (mem == nullptr) {
        puts("oom");
        while (true)
            ;
    }
    mem->object_.class_ = &return_address_class;
    mem->pc_ = pc;

    return mem;
}


#ifndef JVM_OPERAND_STACK_SIZE
#define JVM_OPERAND_STACK_SIZE 1024
#endif


#ifndef JVM_STACK_LOCALS_SIZE
#define JVM_STACK_LOCALS_SIZE 1024
#endif


// TODO: maintain a secondary bitvector, where we can keep track of whether an
// operand stack element is primitive or instance.
Buffer<void*, JVM_OPERAND_STACK_SIZE> __operand_stack;
// For the GC implementation, we'll ultimately need to keep track of which
// operands are objects.
Buffer<bool, JVM_OPERAND_STACK_SIZE> __operand_is_object;

Buffer<void*, JVM_STACK_LOCALS_SIZE> __locals;
Buffer<bool, JVM_STACK_LOCALS_SIZE> __local_is_object;



void store_local(int index, void* value, bool is_object)
{
    __locals[(__locals.size() - 1) - index] = value;
    __local_is_object[(__local_is_object.size() - 1) - index] = is_object;
}


void* load_local(int index)
{
    return __locals[(__locals.size() - 1) - index];
}


void store_wide_local(int index, void* value)
{
    s32* words = (s32*)value;

    store_local(index, (void*)(intptr_t)words[0], false);
    store_local(index + 1, (void*)(intptr_t)words[1], false);
}


void load_wide_local(int index, void* result)
{
    s32* words = (s32*)result;

    words[0] = (s32)(intptr_t)load_local(index);
    words[1] = (s32)(intptr_t)load_local(index + 1);
}


void alloc_locals(int count)
{
    for (int i = 0; i < count; ++i) {
        __locals.push_back(nullptr);
        __local_is_object.push_back(false);
    }
}


void free_locals(int count)
{
    for (int i = 0; i < count; ++i) {
        __locals.pop_back();
        __local_is_object.pop_back();
    }
}


void __push_operand_impl(void* value, bool is_object)
{
    __operand_stack.push_back(value);
    __operand_is_object.push_back(is_object);
}


void push_operand_p(void* value)
{
    __push_operand_impl(value, false);
}


// Soon, when we add garbage collection, we'll need to make a special
// distinction for objects vs primitives on the operand stack, so we should
// remember to push objects with push_operand_a.
void push_operand_a(Object& value)
{
    __push_operand_impl(&value, true);
}


void push_operand_i(s32 value)
{
    __push_operand_impl((void*)(intptr_t)value, false);
}


void __push_operand_f_impl(float* value)
{
    __push_operand_impl((void*)(intptr_t) * (int*)value, false);
}


void push_operand_f(float value)
{
    __push_operand_f_impl(&value);
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


void push_wide_operand(void* value)
{
    s32* words = (s32*)value;
    push_operand_i(words[0]);
    push_operand_i(words[1]);
}


void push_wide_operand_l(s64 value)
{
    push_wide_operand((void*)&value);
}


void push_wide_operand_d(double value)
{
    push_wide_operand((void*)&value);
}


s64 load_wide_operand_l(int offset)
{
    s64 result;
    s32* words = (s32*)&result;
    words[1] = load_operand_i(offset);
    words[0] = load_operand_i(offset + 1);
    return result;
}


double __load_wide_operand_d_impl(void* ptr)
{
    return *(double*)ptr;
}


double load_wide_operand_d(int offset)
{
    static_assert(sizeof(double) == sizeof(u64),
                  "is this even remotely necessary?");

    auto mem = load_wide_operand_l(offset);
    return __load_wide_operand_d_impl(&mem);
}


void dup(int offset)
{
    __operand_stack.push_back(
        __operand_stack[(__operand_stack.size() - 1) - offset]);

    __operand_is_object.push_back(
        __operand_is_object[(__operand_is_object.size() - 1) - offset]);
}


void swap()
{
    std::swap(__operand_stack[__operand_stack.size() - 1],
              __operand_stack[__operand_stack.size() - 2]);

    std::swap(__operand_is_object[__operand_is_object.size() - 1],
              __operand_is_object[__operand_is_object.size() - 2]);
}


void pop_operand()
{
    __operand_stack.pop_back();
    __operand_is_object.pop_back();
}


// clang-format off
struct Bytecode {
    enum : u8 {
        nop             = 0x00,
        pop             = 0x57,
        pop2            = 0x58,
        swap            = 0x5f,
        ldc             = 0x12,
        ldc_w           = 0x13,
        ldc2_w          = 0x14,
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
        athrow          = 0xbf,
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
        iand            = 0x7e,
        ior             = 0x80,
        ixor            = 0x82,
        ishl            = 0x78,
        ishr            = 0x7a,
        iushr           = 0x7c,
        irem            = 0x70,
        iadd            = 0x60,
        isub            = 0x64,
        idiv            = 0x6c,
        imul            = 0x68,
        ineg            = 0x74,
        i2f             = 0x86,
        i2c             = 0x92,
        i2s             = 0x93,
        i2l             = 0x85,
        i2d             = 0x87,
        iinc            = 0x84,
        iastore         = 0x4f,
        iaload          = 0x2e,
        lstore          = 0x37,
        lstore_0        = 0x3f,
        lstore_1        = 0x40,
        lstore_2        = 0x41,
        lstore_3        = 0x42,
        lload           = 0x16,
        lload_0         = 0x1e,
        lload_1         = 0x1f,
        lload_2         = 0x20,
        lload_3         = 0x21,
        lastore         = 0x50,
        laload          = 0x2f,
        lcmp            = 0x94,
        ladd            = 0x61,
        lsub            = 0x65,
        lmul            = 0x69,
        ldiv            = 0x6d,
        lneg            = 0x75,
        land            = 0x7f,
        lor             = 0x81,
        lrem            = 0x71,
        lconst_0        = 0x09,
        lconst_1        = 0x0a,
        lreturn         = 0xad,
        l2d             = 0x8a,
        l2f             = 0x89,
        l2i             = 0x88,
        ireturn         = 0xac,
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
        f2d             = 0x8d,
        f2i             = 0x8b,
        f2l             = 0x8c,
        fneg            = 0x76,
        frem            = 0x72,
        fadd            = 0x62,
        fsub            = 0x66,
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
        dcmpl           = 0x97,
        dcmpg           = 0x98,
        freturn         = 0xae,
        dreturn         = 0xaf,
        d2f             = 0x90,
        d2i             = 0x8e,
        d2l             = 0x8f,
        dadd            = 0x63,
        dsub            = 0x67,
        dmul            = 0x6b,
        ddiv            = 0x6f,
        dneg            = 0x77,
        drem            = 0x73,
        daload          = 0x31,
        dastore         = 0x52,
        dstore          = 0x39,
        dstore_0        = 0x47,
        dstore_1        = 0x48,
        dstore_2        = 0x49,
        dstore_3        = 0x4a,
        dload           = 0x18,
        dload_0         = 0x26,
        dload_1         = 0x27,
        dload_2         = 0x28,
        dload_3         = 0x29,
        dconst_0        = 0x0e,
        dconst_1        = 0x0f,
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
        ret             = 0xa9,
        jsr             = 0xa8,
        jsr_w           = 0xc9,
        monitorenter    = 0xc2,
        monitorexit     = 0xc3,
    };
};
// clang-format on


struct ClassTableEntry {
    Slice name_;
    Class* class_;
};



// FIXME: use a different datastructure for the class table (perhaps an
// intrusive binary tree?). We have no idea how many classes will be in the
// system.
static Buffer<ClassTableEntry, 100> class_table;



Class* import(Slice classpath)
{
    if (jar_file_data == nullptr) {
        puts("missing jar, import failed");
        while (true)
            ;
    }

    auto data = jar::load_classfile(jar_file_data, classpath);
    if (data.length_) {
        if (auto clz = parse_classfile(classpath, data.ptr_)) {
            return clz;
        }
    }
    return nullptr;
}



void register_class(Slice name, Class* clz)
{
    class_table.push_back({name, clz});
}



using Exception = Object;



Exception* execute_bytecode(Class* clz,
                            const u8* bytecode,
                            const ClassFile::ExceptionTable* exception_table);



void pop_arguments(const ArgumentInfo& argc)
{
    for (int i = 0; i < argc.operand_count_; ++i) {
        // puts("pop argument");
        pop_operand();
    }
}



// Move arguments from the operand stack into local variable slots in the stack
// frame.
void bind_arguments(Object* self,
                    const ArgumentInfo& argc,
                    Slice type_signature)
{
    int local_param_index = 0;
    int stack_load_index = argc.operand_count_ - 1;
    if (self) {
        store_local(local_param_index++, self, true);
        stack_load_index--;
    }

    const char* str = type_signature.ptr_;
    if (str) {
        int i = 1;
        while (true) {
            switch (str[i]) {
            default:
                store_local(
                    local_param_index, load_operand(stack_load_index--), false);
                ++local_param_index;
                ++i;
                break;

            case 'J': {
                auto l = load_wide_operand_l(stack_load_index - 1);
                store_wide_local(local_param_index, &l);
                local_param_index += 2;
                stack_load_index -= 2;
                ++i;
                break;
            }

            case 'D': {
                auto d = load_wide_operand_d(stack_load_index - 1);
                store_wide_local(local_param_index, &d);
                local_param_index += 2;
                stack_load_index -= 2;
                ++i;
                break;
            }

            case 'L':
                store_local(
                    local_param_index, load_operand(stack_load_index--), true);
                ++local_param_index;
                while (str[i] not_eq ';') {
                    ++i;
                }
                ++i;
                break;

            case ')':
                // We've moved all of the arguments from the operand stack to
                // the stack frame, now we can pop the arguments off of the
                // operand stack.
                pop_arguments(argc);
                return;
            }
        }
    }
}




Exception* TODO_throw_proper_exception();




Exception* invoke_method(Class* clz,
                         Object* self,
                         const ClassFile::MethodInfo* method,
                         const ArgumentInfo& argc = ArgumentInfo{},
                         Slice type_signature = Slice(nullptr, 0))
{
    for (int i = 0; i < method->attributes_count_.get(); ++i) {
        auto attr = (ClassFile::AttributeInfo*)((const char*)method +
                                                sizeof(ClassFile::MethodInfo));


        if (attr->attribute_name_index_.get() == jni::magic and
            attr->attribute_length_.get() == jni::magic) {

            alloc_locals(argc.operand_count_);

            bind_arguments(self, argc, type_signature);
            ((jni::MethodStub*)method)->implementation_();

            free_locals(argc.operand_count_);

            return nullptr;

        } else if (clz->constants_->load_string(
                       attr->attribute_name_index_.get()) ==
                   Slice::from_c_str("Code")) {

            auto bytecode =
                ((const u8*)attr) + sizeof(ClassFile::AttributeCode);

            auto exception_table =
                (const ClassFile::ExceptionTable*)(bytecode +
                                                   ((ClassFile::AttributeCode*)
                                                        attr)
                                                       ->code_length_.get());

            const auto local_count =
                std::max(((ClassFile::AttributeCode*)attr)->max_locals_.get(),
                         (u16)4); // Why a min of four? istore_0-3, so there
                                  // must be at least four slots.

            alloc_locals(local_count);

            bind_arguments(self, argc, type_signature);
            auto exn = execute_bytecode(clz, bytecode, exception_table);

            free_locals(local_count);

            return exn;
        }
    }

    return TODO_throw_proper_exception();
}



Class* load_class_by_name(Slice class_name)
{
    for (auto& entry : class_table) {
        if (entry.name_ == class_name) {
            return entry.class_;
        }
    }

    if (auto clz = import(class_name)) {
        return clz;
    } else {
        puts("missing class!");
        while (true)
            ;
    }
}



Class* load_class(Class* current_module, u16 class_index)
{
    auto c_clz =
        (const ClassFile::ConstantClass*)current_module->constants_->load(
            class_index);

    auto cname =
        current_module->constants_->load_string(c_clz->name_index_.get());


    return load_class_by_name(cname);
}



std::pair<const ClassFile::MethodInfo*, Class*>
lookup_method(Class* clz, Slice lhs_name, Slice lhs_type)
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



static Exception* dispatch_method(Class* clz,
                                  u16 method_index,
                                  bool direct_dispatch,
                                  bool special)
{
    auto ref =
        (const ClassFile::ConstantRef*)clz->constants_->load(method_index);

    auto nt = (const ClassFile::ConstantNameAndType*)clz->constants_->load(
        ref->name_and_type_index_.get());

    auto lhs_name = clz->constants_->load_string(nt->name_index_.get());
    auto lhs_type = clz->constants_->load_string(nt->descriptor_index_.get());

    auto argc = parse_arguments(lhs_type);

    Object* self = nullptr;
    if ((not direct_dispatch) or special) {
        self = (Object*)load_operand(argc.operand_count_);

        if (self == nullptr) {
            // We're exiting early. Usually, a further nested function call
            // removes arguments from the caller's operand stack and copies them
            // into the callee's stack frame. But because we hit a
            // NullPointerException when trying to invoke a method, we need to
            // pop the arguments off the the stack before exiting.

            for (int i = 0;
                 i < argc.operand_count_ + 1; // +1 for self
                 ++i) {
                pop_operand();
            }

            return TODO_throw_proper_exception();
        }
    }

    auto mtd = [&] {
        if (not direct_dispatch) {
            return lookup_method(self->class_, lhs_name, lhs_type);
        } else {
            // If we do not have a self pointer we're looking up a static
            // method, if direct_dispatch, we're processing an invoke_special
            // instruction.
            Class* t_clz = load_class(clz, ref->class_index_.get());
            if (t_clz == nullptr) {
                printf("failed to load class %d, TODO: raise error...\n",
                       ref->class_index_.get());
                while (true)
                    ;
            }
            return lookup_method(t_clz, lhs_name, lhs_type);
        }
    }();

    if (mtd.first) {
        if (self) {
            argc.operand_count_ += 1;
        }
        return invoke_method(mtd.second, self, mtd.first, argc, lhs_type);
    } else {
        return TODO_throw_proper_exception();
    }
}



Exception* invoke_special(Class* clz, u16 method_index)
{
    return dispatch_method(clz, method_index, true, true);
}



Object* make_instance_impl(Class* clz)
{
    const auto fields_size = clz->instance_fields_size();

    // printf("instance size %ld\n", fields_size);

    auto mem = (Object*)heap::allocate(sizeof(Object) + fields_size);
    if (mem == nullptr) {
        puts("oom");
        while (true)
            ;
    }
    new (mem) Object();
    mem->class_ = clz;
    return mem;
}




Object* make_instance(Class* current_module, u16 class_constant)
{
    auto clz = load_class(current_module, class_constant);

    if (clz) {
        return make_instance_impl(clz);
    }

    // TODO: fatal error...
    puts("warning! failed to alloc class!");
    return nullptr;
}



Exception* TODO_throw_proper_exception()
{
    return make_instance_impl(load_class_by_name(Slice::from_c_str("java/lang/Throwable")));
}



void ldc1(Class* clz, u16 index)
{
    auto c = clz->constants_->load(index);
    switch (c->tag_) {
    case ClassFile::ConstantType::t_float: {
        auto cfl = (ClassFile::ConstantFloat*)c;
        float result;
        auto input = cfl->value_.get();
        memcpy(&result, &input, sizeof(float));
        push_operand_f(result);
        break;
    }

    case ClassFile::ConstantType::t_integer: {
        auto cint = (ClassFile::ConstantInteger*)c;
        push_operand_i(cint->value_.get());
        break;
    }

    default:
        puts("unhandled ldc...");
        while (true)
            ;
        break;
    }
}



static void __push_double_from_aligned_bytevector(void* d)
{
    push_wide_operand_d(*(double*)d);
}



void ldc2(Class* clz, u16 index)
{
    auto c = clz->constants_->load(index);
    switch (c->tag_) {
    case ClassFile::ConstantType::t_double: {
        auto cdbl = (ClassFile::ConstantDouble*)c;
        u64 val{cdbl->value_.get()};
        __push_double_from_aligned_bytevector(&val);
        break;
    }

    case ClassFile::ConstantType::t_long: {
        auto clong = (ClassFile::ConstantLong*)c;
        push_wide_operand_l(clong->value_.get());
        break;
    }

    default:
        puts("invalid ldc...");
        while (true)
            ;
        break;
    }
}



bool instanceof (Object * obj, Class* clz)
{
    auto current = obj->class_;
    while (current) {
        // FIXME: check for successful cast to implemented interfaces
        if (current == clz) {
            return true;
        }
        current = current->super_;
    }
    return false;
}



const ClassFile::ExceptionTableEntry*
find_exception_handler(Class* clz,
                       Object* exception,
                       u32 pc,
                       const ClassFile::ExceptionTable* exception_table)
{
    if (exception_table == nullptr) {
        return nullptr;
    }

    for (int i = 0; i < exception_table->exception_table_length_.get(); ++i) {
        auto& entry = exception_table->entries()[i];

        if (pc >= entry.start_pc_.get() and pc < entry.end_pc_.get()) {
            auto catch_clz = load_class(clz, entry.catch_type_.get());
            if (instanceof (exception, catch_clz)) {
                return &entry;
            }
        }
    }

    return nullptr;
}



bool handle_exception(Class* clz,
                      Exception* exn,
                      u32& pc,
                      const ClassFile::ExceptionTable* exception_table)
{
    auto handler = find_exception_handler(clz, exn, pc, exception_table);
    if (handler) {
        // Subsequent bytecode will expect to be able to load the
        // exception object into a local variable, so push it onto
        // the stack.
        push_operand_a(*(Object*)exn);
        pc = handler->handler_pc_.get();
        return true;

    } else {
        return false;
    }
}



Exception* execute_bytecode(Class* clz,
                            const u8* bytecode,
                            const ClassFile::ExceptionTable* exception_table)
{
    u32 pc = 0;

    while (true) {
        // printf("vm loop %d %x\n", pc, bytecode[pc]);
        switch (bytecode[pc]) {
        case Bytecode::nop:
            ++pc;
            break;

        case Bytecode::pop:
            pop_operand();
            ++pc;
            break;

        case Bytecode::pop2:
            pop_operand();
            pop_operand();
            ++pc;
            break;

        case Bytecode::swap:
            swap();
            ++pc;
            break;

        case Bytecode::ldc:
            ldc1(clz, bytecode[pc + 1]);
            pc += 2;
            break;

        case Bytecode::ldc_w:
            ldc1(clz, ((network_u16*)&bytecode[pc + 1])->get());
            pc += 3;
            break;

        case Bytecode::ldc2_w:
            ldc2(clz, ((network_u16*)&bytecode[pc + 1])->get());
            pc += 3;
            break;

        case Bytecode::new_inst:
            push_operand_a(
                *make_instance(clz, ((network_u16*)&bytecode[pc + 1])->get()));
            pc += 3;
            break;

        case Bytecode::bipush:
            push_operand_i((s32)bytecode[pc + 1]);
            pc += 2;
            break;

        case Bytecode::anewarray: {
            auto len = load_operand_i(0);
            pop_operand();

            auto array = Array::create(len, sizeof(Object*));

            if (array == nullptr) {
                puts("TODO: ran out of memory, TODO: throw oom");
                while (true)
                    ;
            }

            array->object_.class_ = &reference_array_class;
            push_operand_a(*(Object*)array);
            pc += 3;
            break;
        }

        case Bytecode::newarray: {
            const int element_count = load_operand_i(0);
            pop_operand();

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

            auto array = Array::create(element_count, element_size);

            array->object_.class_ = &primitive_array_class;
            push_operand_a(*(Object*)array);
            pc += 2;
            break;
        }

        case Bytecode::arraylength: {
            auto array = (Array*)load_operand(0);
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            push_operand_i(array->size_);
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

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                Object* result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand_a(*result);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
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

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            pop_operand();
            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::aconst_null:
            push_operand_p((void*)nullptr);
            pc += 1;
            break;

        case Bytecode::athrow: {
            THROW:
            auto exn = (Object*)load_operand(0);
            pop_operand();

            if (not handle_exception(clz, exn, pc, exception_table)) {
                return exn;
            }
            break;
        }

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
            while (true)
                ;

        CAST_SUCCESS:
            push_operand_a(*obj);
            pc += 3;
            break;
        }

        case Bytecode:: instanceof: {
            auto other =
                load_class(clz, ((network_u16*)&bytecode[pc + 1])->get());
            auto obj = (Object*)load_operand(0);
            pop_operand();
            pc += 3;
            push_operand_i(instanceof (obj, other));
            break;
        }

        case Bytecode::dup:
            dup(0);
            ++pc;
            break;

        case Bytecode::dup2:
            // FiXME: loses info about whether operands are objects.
            dup(1);
            dup(1);
            ++pc;
            break;

        case Bytecode::iconst_m1:
            push_operand_i(-1);
            ++pc;
            break;

        case Bytecode::iconst_0:
            push_operand_i(0);
            ++pc;
            break;

        case Bytecode::iconst_1:
            push_operand_i(1);
            ++pc;
            break;

        case Bytecode::iconst_2:
            push_operand_i(2);
            ++pc;
            break;

        case Bytecode::iconst_3:
            push_operand_i(3);
            ++pc;
            break;

        case Bytecode::iconst_4:
            push_operand_i(4);
            ++pc;
            break;

        case Bytecode::iconst_5:
            push_operand_i(5);
            ++pc;
            break;

        case Bytecode::getfield: {
            // FIXME: getfield and putfield need to be refactored so that they
            // check the size of the field before trying to read/write it, so
            // that we know how many operand stack slots that we need to
            // read. The code currently works for most datatypes, but breaks for
            // long/double datatypes.

            auto arg = (Object*)load_operand(0);
            pop_operand();

            if (arg == nullptr) {
                // TODO: NullPointerException
                return TODO_throw_proper_exception();
            }

            bool is_object;
            auto field = arg->get_field(
                ((network_u16*)&bytecode[pc + 1])->get(), is_object);

            if (is_object) {
                push_operand_a(*(Object*)field);
            } else {
                push_operand_p(field);
            }

            pc += 3;
            break;
        }

        case Bytecode::putfield: {
            auto obj = (Object*)load_operand(1);
            auto val = load_operand(0);

            pop_operand();
            pop_operand();

            if (obj == nullptr) {
                // TODO: NullPointerException
                return TODO_throw_proper_exception();
            }

            obj->put_field(((network_u16*)&bytecode[pc + 1])->get(),
                           val);
            pc += 3;
            break;
        }

        case Bytecode::isub: {
            const s32 result = load_operand_i(1) - load_operand_i(0);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::iand: {
            const s32 result = load_operand_i(0) & load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::ior: {
            const s32 result = load_operand_i(0) | load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::ixor: {
            const s32 result = load_operand_i(0) ^ load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::iadd: {
            const s32 result = load_operand_i(0) + load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::idiv: {
            auto value = load_operand_i(1);
            auto divisor = load_operand_i(0);

            pop_operand();
            pop_operand();

            if (divisor == 0) {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            const s32 result = value / divisor;

            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::irem: {
            const s32 result = load_operand_i(1) % load_operand_i(0);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::iushr: {
            const s32 result = (u32)load_operand_i(1)
                               << ((u32)load_operand_i(0) & 0x1f);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        // FIXME: ishl and ishr may not be implemented correction (sign
        // extension and negative numbers).
        case Bytecode::ishl: {
            const s32 result = load_operand_i(1) << (load_operand_i(0) & 0x1f);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::ishr: {
            const s32 result = load_operand_i(1) >> (load_operand_i(0) & 0x1f);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::imul: {
            const s32 result = load_operand_i(1) * load_operand_i(0);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::ineg: {
            const s32 result = -load_operand_i(0);
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::i2c: {
            u8 val = load_operand_i(0);
            pop_operand();
            push_operand_i(val);
            ++pc;
            break;
        }

        case Bytecode::i2f: {
            float val = load_operand_i(0);
            pop_operand();
            push_operand_f(val);
            ++pc;
            break;
        }

        case Bytecode::i2s: {
            s16 val = load_operand_i(0);
            pop_operand();
            push_operand_i(val);
            ++pc;
            break;
        }

        case Bytecode::i2l: {
            s64 val = load_operand_i(0);
            pop_operand();
            push_wide_operand_l(val);
            ++pc;
            break;
        }

        case Bytecode::i2d: {
            double val = load_operand_i(0);
            pop_operand();
            push_wide_operand_d(val);
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
            if (load_operand_i(0) > load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmpge:
            if (load_operand_i(0) < load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmple:
            if (load_operand_i(0) >= load_operand_i(1)) {
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
            push_operand_f(f);
            ++pc;
            break;
        }

        case Bytecode::fconst_1: {
            static_assert(sizeof(float) == sizeof(s32) and
                              sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 1.f;
            push_operand_f(f);
            ++pc;
            break;
        }

        case Bytecode::fconst_2: {
            static_assert(sizeof(float) == sizeof(s32) and
                              sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 2.f;
            push_operand_f(f);
            ++pc;
            break;
        }

        case Bytecode::f2d: {
            float value = load_operand_f(0);
            pop_operand();
            push_wide_operand_d(value);
            ++pc;
            break;
        }

        case Bytecode::f2i: {
            float value = load_operand_f(0);
            pop_operand();
            push_operand_i(value);
            ++pc;
            break;
        }

        case Bytecode::f2l: {
            float value = load_operand_f(0);
            pop_operand();
            push_wide_operand_l(value);
            ++pc;
            break;
        }

        case Bytecode::fneg: {
            float value = load_operand_f(0);
            pop_operand();
            push_operand_f(-value);
            ++pc;
            break;
        }

        case Bytecode::frem: {
            float lhs = load_operand_f(1);
            float rhs = load_operand_f(0);
            pop_operand();
            pop_operand();
            push_operand_f(fmod(lhs, rhs));
            ++pc;
            break;
        }

        case Bytecode::fsub: {
            float lhs = load_operand_f(1);
            float rhs = load_operand_f(0);
            pop_operand();
            pop_operand();
            auto result = lhs - rhs;
            push_operand_f(result);
            ++pc;
        }

        case Bytecode::fadd: {
            float lhs = load_operand_f(0);
            float rhs = load_operand_f(1);
            pop_operand();
            pop_operand();
            auto result = lhs + rhs;
            push_operand_f(result);
            ++pc;
            break;
        }

        case Bytecode::fdiv: {
            float lhs = load_operand_f(1);
            float rhs = load_operand_f(0);
            pop_operand();
            pop_operand();
            auto result = lhs / rhs;
            push_operand_f(result);
            ++pc;
            break;
        }

        case Bytecode::fmul: {
            float lhs = load_operand_f(0);
            float rhs = load_operand_f(1);
            pop_operand();
            pop_operand();
            auto result = lhs * rhs;
            push_operand_f(result);
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
                push_operand_i(1);
            } else if (lhs == rhs) {
                push_operand_i(0);
            } else if (lhs < rhs) {
                push_operand_i(-1);
            } else {
                if (bytecode[pc] == Bytecode::fcmpg) {
                    push_operand_i(1);
                } else {
                    push_operand_i(-1);
                }
            }
            ++pc;
            break;
        }

        case Bytecode::dcmpg:
        case Bytecode::dcmpl: {
            auto rhs = load_wide_operand_d(0);
            auto lhs = load_wide_operand_d(2);

            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();

            if (lhs > rhs) {
                push_operand_i(1);
            } else if (lhs == rhs) {
                push_operand_i(0);
            } else if (lhs < rhs) {
                push_operand_i(-1);
            } else {
                if (bytecode[pc] == Bytecode::dcmpg) {
                    push_operand_i(1);
                } else {
                    push_operand_i(-1);
                }
            }
            ++pc;
            break;
        }

        case Bytecode::d2f: {
            double d = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_operand_f(d);
            ++pc;
            break;
        }

        case Bytecode::d2i: {
            double d = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_operand_i(d);
            ++pc;
            break;
        }

        case Bytecode::d2l: {
            double d = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_wide_operand_l(d);
            ++pc;
            break;
        }

        case Bytecode::dadd: {
            auto rhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            auto lhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_wide_operand_d(lhs + rhs);
            ++pc;
            break;
        }

        case Bytecode::dsub: {
            auto rhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            auto lhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_wide_operand_d(lhs - rhs);
            ++pc;
            break;
        }

        case Bytecode::dmul: {
            auto rhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            auto lhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_wide_operand_d(lhs * rhs);
            ++pc;
            break;
        }

        case Bytecode::ddiv: {
            auto rhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            auto lhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_wide_operand_d(lhs / rhs);
            ++pc;
            break;
        }

        case Bytecode::drem: {
            auto rhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            auto lhs = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_wide_operand_d(fmod(lhs, rhs));
            ++pc;
            break;
        }

        case Bytecode::dneg: {
            auto value = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            push_wide_operand_d(-value);
            ++pc;
            break;
        }

        case Bytecode::daload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);
            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                double result;
                memcpy(&result, array->address(index), sizeof result);
                push_wide_operand_d(result);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::dastore: {
            auto value = load_wide_operand_d(0);
            auto index = load_operand_i(2);
            auto array = (Array*)load_operand(3);

            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::dload: {
            double val;
            load_wide_local(bytecode[pc + 1], &val);
            push_wide_operand_d(val);
            pc += 2;
            break;
        }

        case Bytecode::dload_0: {
            double val;
            load_wide_local(0, &val);
            push_wide_operand_d(val);
            ++pc;
            break;
        }

        case Bytecode::dload_1: {
            double val;
            load_wide_local(1, &val);
            push_wide_operand_d(val);
            ++pc;
            break;
        }

        case Bytecode::dload_2: {
            double val;
            load_wide_local(2, &val);
            push_wide_operand_d(val);
            ++pc;
            break;
        }

        case Bytecode::dload_3: {
            double val;
            load_wide_local(3, &val);
            push_wide_operand_d(val);
            ++pc;
            break;
        }

        case Bytecode::dconst_0: {
            push_wide_operand_d(0.0);
            ++pc;
            break;
        }

        case Bytecode::dconst_1: {
            push_wide_operand_d(1.0);
            ++pc;
            break;
        }

        case Bytecode::dstore: {
            auto val = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            store_wide_local(bytecode[pc + 1], &val);
            pc += 2;
            break;
        }

        case Bytecode::dstore_0: {
            auto val = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            store_wide_local(0, &val);
            ++pc;
            break;
        }

        case Bytecode::dstore_1: {
            auto val = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            store_wide_local(1, &val);
            ++pc;
            break;
        }

        case Bytecode::dstore_2: {
            auto val = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            store_wide_local(2, &val);
            ++pc;
            break;
        }

        case Bytecode::dstore_3: {
            auto val = load_wide_operand_d(0);
            pop_operand();
            pop_operand();
            store_wide_local(3, &val);
            ++pc;
            break;
        }

        case Bytecode::laload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);
            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                s64 result;
                memcpy(&result, array->address(index), sizeof result);
                push_wide_operand_l(result);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::lastore: {
            auto value = load_wide_operand_l(0);
            auto index = load_operand_i(2);
            auto array = (Array*)load_operand(3);

            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
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

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                s16 result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand_i(result);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
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

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::sipush: {
            s16 val = ((network_s16*)(bytecode + pc + 1))->get();
            push_operand_i(val);
            pc += 3;
            break;
        }

        case Bytecode::astore:
            store_local(bytecode[pc + 1], load_operand(0), true);
            pop_operand();
            pc += 2;
            break;

        case Bytecode::astore_0:
            store_local(0, load_operand(0), true);
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_1:
            store_local(1, load_operand(0), true);
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_2:
            store_local(2, load_operand(0), true);
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_3:
            store_local(3, load_operand(0), true);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore:
        case Bytecode::istore:
            store_local(bytecode[pc + 1], load_operand(0), false);
            pop_operand();
            pc += 2;
            break;

        case Bytecode::fstore_0:
        case Bytecode::istore_0:
            store_local(0, load_operand(0), false);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_1:
        case Bytecode::istore_1:
            store_local(1, load_operand(0), false);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_2:
        case Bytecode::istore_2:
            store_local(2, load_operand(0), false);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_3:
        case Bytecode::istore_3:
            store_local(3, load_operand(0), false);
            pop_operand();
            ++pc;
            break;

        case Bytecode::aload:
            push_operand_a(*(Object*)load_local(bytecode[pc + 1]));
            pc += 2;
            break;

        case Bytecode::aload_0:
            push_operand_a(*(Object*)load_local(0));
            ++pc;
            break;

        case Bytecode::aload_1:
            push_operand_a(*(Object*)load_local(1));
            ++pc;
            break;

        case Bytecode::aload_2:
            push_operand_a(*(Object*)load_local(2));
            ++pc;
            break;

        case Bytecode::aload_3:
            push_operand_a(*(Object*)load_local(3));
            ++pc;
            break;

        case Bytecode::fload:
        case Bytecode::iload:
            push_operand_p(load_local(bytecode[pc + 1]));
            pc += 2;
            break;

        case Bytecode::fload_0:
        case Bytecode::iload_0:
            push_operand_p(load_local(0));
            ++pc;
            break;

        case Bytecode::fload_1:
        case Bytecode::iload_1:
            push_operand_p(load_local(1));
            ++pc;
            break;

        case Bytecode::fload_2:
        case Bytecode::iload_2:
            push_operand_p(load_local(2));
            ++pc;
            break;

        case Bytecode::fload_3:
        case Bytecode::iload_3:
            push_operand_p(load_local(3));
            ++pc;
            break;

        case Bytecode::iinc:
            store_local(bytecode[pc + 1],
                        (void*)(intptr_t)(
                            (int)(intptr_t)(load_local(bytecode[pc + 1])) +
                            bytecode[pc + 2]),
                        false);
            pc += 3;
            break;

        case Bytecode::castore: {
            auto array = (Array*)load_operand(2);
            u8 value = load_operand_i(0);
            s32 index = load_operand_i(1);

            pop_operand();
            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                *array->address(index) = value;
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::bastore: {
            auto array = (Array*)load_operand(2);
            s8 value = load_operand_i(0);
            s32 index = load_operand_i(1);

            pop_operand();
            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                *array->address(index) = value;
            } else {
                puts("out of bounds!?");
                while (1) ;
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::caload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);

            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                u8 value = *array->address(index);
                push_operand_i(value);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::baload: {
            auto array = (Array*)load_operand(1);
            s32 index = load_operand_i(0);

            pop_operand();
            pop_operand();

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                s8 value = *array->address(index);
                push_operand_i(value);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
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

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
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

            if (array == nullptr) {
                puts("TODO: nullptr exception");
                while (true)
                    ;
            }

            if (array->check_bounds(index)) {
                s32 result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand_i(result);
            } else {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }
            ++pc;
            break;
        }

        case Bytecode::lconst_0: {
            s64 value = 0;
            push_wide_operand_l(value);
            ++pc;
            break;
        }

        case Bytecode::lconst_1: {
            s64 value = 1;
            push_wide_operand_l(value);
            ++pc;
            break;
        }

        case Bytecode::l2d: {
            double arg = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            push_wide_operand_d(arg);
            ++pc;
            break;
        }

        case Bytecode::l2f: {
            float arg = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            push_operand_f(arg);
            ++pc;
            break;
        }

        case Bytecode::l2i: {
            s32 arg = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            push_operand_i(arg);
            ++pc;
            break;
        }

        case Bytecode::lstore: {
            auto val = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            store_wide_local(bytecode[pc + 1], &val);
            pc += 2;
            break;
        }

        case Bytecode::lstore_0: {
            auto val = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            store_wide_local(0, &val);
            ++pc;
            break;
        }

        case Bytecode::lstore_1: {
            auto val = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            store_wide_local(1, &val);
            ++pc;
            break;
        }

        case Bytecode::lstore_2: {
            auto val = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            store_wide_local(2, &val);
            ++pc;
            break;
        }

        case Bytecode::lstore_3: {
            auto val = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            store_wide_local(3, &val);
            ++pc;
            break;
        }

        case Bytecode::lload: {
            s64 val;
            load_wide_local(bytecode[pc + 1], &val);
            push_wide_operand_l(val);
            pc += 2;
            break;
        }

        case Bytecode::lload_0: {
            s64 val;
            load_wide_local(0, &val);
            push_wide_operand_l(val);
            ++pc;
            break;
        }

        case Bytecode::lload_1: {
            s64 val;
            load_wide_local(1, &val);
            push_wide_operand_l(val);
            ++pc;
            break;
        }

        case Bytecode::lload_2: {
            s64 val;
            load_wide_local(2, &val);
            push_wide_operand_l(val);
            ++pc;
            break;
        }

        case Bytecode::lload_3: {
            s64 val;
            load_wide_local(3, &val);
            push_wide_operand_l(val);
            ++pc;
            break;
        }

        case Bytecode::lcmp: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            if (lhs == rhs) {
                push_operand_i(0);
            } else if (lhs > rhs) {
                push_operand_i(1);
            } else {
                push_operand_i(-1);
            }
            ++pc;
            break;
        }

        case Bytecode::lsub: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs - rhs);
            ++pc;
            break;
        }

        case Bytecode::ldiv: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();

            if (rhs == 0) {
                push_operand_a(*(Object*)TODO_throw_proper_exception());
                goto THROW;
            }

            push_wide_operand_l(lhs / rhs);
            ++pc;
            break;
        }

        case Bytecode::ladd: {
            auto lhs = load_wide_operand_l(0);
            auto rhs = load_wide_operand_l(2);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs + rhs);
            ++pc;
            break;
        }

        case Bytecode::lmul: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs * rhs);
            ++pc;
            break;
        }

        case Bytecode::lneg: {
            auto value = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            push_wide_operand_l(-value);
            ++pc;
            break;
        }

        case Bytecode::land: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs & rhs);
            ++pc;
            break;
        }

        case Bytecode::lor: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs | rhs);
            ++pc;
            break;
        }

        case Bytecode::lrem: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs % rhs);
            ++pc;
            break;
        }

        case Bytecode::__goto:
            pc += ((network_s16*)(bytecode + pc + 1))->get();
            break;

        case Bytecode::__goto_w:
            pc += ((network_s32*)(bytecode + pc + 1))->get();
            break;

        case Bytecode::vreturn:
        case Bytecode::lreturn:
        case Bytecode::ireturn:
        case Bytecode::areturn:
        case Bytecode::freturn:
        case Bytecode::dreturn:
            return nullptr;

        case Bytecode::invokestatic: {
            auto exn = dispatch_method(
                clz, ((network_u16*)(bytecode + pc + 1))->get(), true, false);
            if (exn) {
                if (not handle_exception(clz, exn, pc, exception_table)) {
                    return exn;
                }
            } else {
                pc += 3;
            }
            break;
        }

        case Bytecode::invokevirtual: {
            auto exn = dispatch_method(
                clz, ((network_u16*)(bytecode + pc + 1))->get(), false, false);
            if (exn) {
                if (not handle_exception(clz, exn, pc, exception_table)) {
                    return exn;
                }
            } else {
                pc += 3;
            }
            break;
        }

        case Bytecode::invokeinterface: {
            auto exn = dispatch_method(
                clz, ((network_u16*)(bytecode + pc + 1))->get(), false, false);
            if (exn) {
                if (not handle_exception(clz, exn, pc, exception_table)) {
                    return exn;
                }
            } else {
                pc += 5;
            }
            break;
        }

        case Bytecode::invokespecial: {
            auto exn =
                invoke_special(clz, ((network_u16*)(bytecode + pc + 1))->get());
            if (exn) {
                if (not handle_exception(clz, exn, pc, exception_table)) {
                    return exn;
                }
            } else {
                pc += 3;
            }
            break;
        }

        // NOTE: jsr, jsr_w, and ret are untested. I need to find a java
        // compiler that actually generates them.
        case Bytecode::jsr: {
            push_operand_a(*(Object*)make_return_address(pc + 3));
            pc += ((network_s16*)(bytecode + pc + 1))->get();
            break;
        }

        case Bytecode::jsr_w: {
            push_operand_a(*(Object*)make_return_address(pc + 5));
            pc += ((network_s32*)(bytecode + pc + 1))->get();
            break;
        }

        case Bytecode::ret: {
            auto rt = (ReturnAddress*)load_local(bytecode[pc + 1]);
            pc = rt->pc_;
            break;
        }

        // NOTE: We intend our JVM implementation for embedded systems, where we
        // do not care about multithreaded execution. monitorenter/exit
        // essentially do nothing.
        case Bytecode::monitorenter:
            pop_operand();
            ++pc;
            break;

        case Bytecode::monitorexit:
            pop_operand();
            ++pc;
            break;

        default:
            printf("unrecognized bytecode instruction %#02x\n", bytecode[pc]);
            for (int i = 0; i < 10; ++i) {
                std::cout << (int)bytecode[i] << std::endl;
            }
            while (true)
                ;
        }
    }
}



INCBIN(object_class, "Object.class");
INCBIN(runtime_class, "Runtime.class");
INCBIN(throwable_class, "Throwable.class");


Object* runtime = nullptr;



void bootstrap()
{
    // NOTE: I manually edited the bytecode in the Object classfile, which is
    // why I do not provide the source code. It's hand-rolled java bytecode.
    if (auto obj_class = parse_classfile(Slice::from_c_str("java/lang/Object"),
                                         (const char*)object_class_data)) {
        puts("successfully loaded Object root!");

        obj_class->super_ = nullptr;

        primitive_array_class.super_ = obj_class;
        reference_array_class.super_ = obj_class;
        return_address_class.super_ = obj_class;
    }

    if (auto runtime_class =
            parse_classfile(Slice::from_c_str("java/lang/Runtime"),
                            (const char*)runtime_class_data)) {

        runtime = make_instance_impl(runtime_class);

        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("getRuntime"),
                                Slice::from_c_str("TODO_:)"),
                                [] { push_operand_a(*runtime); });


        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("exit"),
                                Slice::from_c_str("TODO_:)"),
                                [] { exit((intptr_t)load_local(1)); });


        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("gc"),
                                Slice::from_c_str("TODO_:)"),
                                [] {
                                    puts("TODO: gc");
                                    while (true)
                                        ;
                                });

        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("totalMemory"),
                                Slice::from_c_str("TODO_:)"),
                                [] {
                                    push_wide_operand_l(jvm::heap::total());
                                });

        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("freeMemory"),
                                Slice::from_c_str("TODO_:)"),
                                [] {
                                    push_wide_operand_l(jvm::heap::total() -
                                                        jvm::heap::used());
                                });

    }

    if (parse_classfile(Slice::from_c_str("java/lang/Throwable"),
                        (const char*)throwable_class_data)) {
    }
}



int start(Class* entry_point)
{
    const auto type_signature = Slice::from_c_str("([Ljava/lang/String;)V");


    if (auto entry = entry_point->load_method("main", type_signature)) {

        push_operand_p(nullptr); // String[] args

        ArgumentInfo argc;
        argc.argument_count_ = 1;
        argc.operand_count_ = 1;

        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        auto exn = java::jvm::invoke_method(
            entry_point, nullptr, entry, argc, type_signature);
        auto end = high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<nanoseconds>(end - start).count()
                  << std::endl;

        heap::print_stats([](const char* str) {
            printf("%s", str);
        });

        if (exn) {
            puts("uncaught exception from main method");
            return 1;
        }

        return 0;
    }

    return 1;
}



int start_from_jar(const char* jar_file_bytes, Slice classpath)
{
    jar_file_data = jar_file_bytes;

    bootstrap();

    if (auto clz = java::jvm::import(classpath)) {
        return start(clz);
    } else {
        puts("failed to import main class");
        return 1;
    }
}



int start_from_classfile(const char* class_file_bytes, Slice classpath)
{
    bootstrap();

    if (auto clz = parse_classfile(classpath, class_file_bytes)) {
        return start(clz);
    } else {
        puts("failed to import main class");
        return 1;
    }
}



} // namespace jvm



} // namespace java



////////////////////////////////////////////////////////////////////////////////



#include <string>
#include <fstream>
#include <streambuf>


int main(int argc, char** argv)
{
    if (argc != 3) {
        puts("usage: java <jar|classfile> <classpath>");
        return 1;
    }

    std::string fname(argv[1]);

    std::ifstream t(fname);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());

    const auto classpath = java::Slice::from_c_str(argv[2]);

    if (fname.substr(fname.find_last_of(".") + 1) == "jar") {
        java::jvm::start_from_jar(str.c_str(), classpath);
    } else {
        java::jvm::start_from_classfile(str.c_str(), classpath);
    }

    return 0;
}
