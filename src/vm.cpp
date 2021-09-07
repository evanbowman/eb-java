#include "array.hpp"
#include "class.hpp"
#include "classfile.hpp"
#include "classtable.hpp"
#include "endian.hpp"
#include "gc.hpp"
#include "jar.hpp"
#include "jni.hpp"
#include "object.hpp"
#include "returnAddress.hpp"
#include <iostream>
#include <string.h>
#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include "incbin.h"
#include "memory.hpp"
#include <chrono>
#include <math.h>



namespace java {
namespace jvm {



struct AvailableJar {
    const char* jar_file_data_;
    AvailableJar* next_;
};



static AvailableJar* jars;



void bind_jar(const char* jar_file_data)
{
    auto info = classmemory::allocate<AvailableJar>();

    info->jar_file_data_ = jar_file_data;
    info->next_ = jars;
    jars = info;
}



// Our implementation includes three classes of pseudo-objects, which need to be
// handled separately from all other java objects.
Class primitive_array_class;
Class reference_array_class;
Class return_address_class;



[[noreturn]] void unhandled_error(const char* description)
{
    puts(description);
    while (1)
        ;
}



ReturnAddress* make_return_address(u32 pc)
{
    auto mem = (ReturnAddress*)heap::allocate(sizeof(ReturnAddress));
    if (mem == nullptr) {
        unhandled_error("oom");
    }
    new (mem) ReturnAddress(&return_address_class, pc);

    return mem;
}



OperandStack __operand_stack;
// We need to keep track of operand types, for garbage collection purposes, as
// well as for putfield (which needs to know whether a stack operand is
// long/double).
OperandTypes __operand_types;



OperandStack& operand_stack()
{
    return __operand_stack;
}



OperandTypes& operand_types()
{
    return __operand_types;
}



Locals __locals;
LocalTypes __local_types;



Locals& locals()
{
    return __locals;
}



LocalTypes& local_types()
{
    return __local_types;
}



void store_local(int index, void* value, OperandTypeCategory tp)
{
    __locals[(__locals.size() - 1) - index] = value;
    __local_types[(__local_types.size() - 1) - index] = tp;
}



void* load_local(int index)
{
    return __locals[(__locals.size() - 1) - index];
}



void store_wide_local(int index, void* value)
{
    s32* words = (s32*)value;

    store_local(
        index, (void*)(intptr_t)words[0], OperandTypeCategory::primitive_wide);

    store_local(index + 1,
                (void*)(intptr_t)words[1],
                OperandTypeCategory::primitive_wide);
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
        __local_types.push_back(OperandTypeCategory::primitive);
    }
}



void free_locals(int count)
{
    for (int i = 0; i < count; ++i) {
        __locals.pop_back();
        __local_types.pop_back();
    }
}



void __push_operand_impl(void* value, OperandTypeCategory tp)
{
    __operand_stack.push_back(value);
    __operand_types.push_back(tp);
}



void push_operand_p(void* value)
{
    __push_operand_impl(value, OperandTypeCategory::primitive);
}


// Soon, when we add garbage collection, we'll need to make a special
// distinction for objects vs primitives on the operand stack, so we should
// remember to push objects with push_operand_a.
void push_operand_a(Object& value)
{
    __push_operand_impl(&value, OperandTypeCategory::object);
}



void push_operand_i(s32 value)
{
    __push_operand_impl((void*)(intptr_t)value, OperandTypeCategory::primitive);
}



void __push_operand_f_impl(float* value)
{
    __push_operand_impl((void*)(intptr_t) * (int*)value,
                        OperandTypeCategory::primitive);
}



void push_operand_f(float value)
{
    __push_operand_f_impl(&value);
}



void* load_operand(int offset)
{
    return __operand_stack[(__operand_stack.size() - 1) - offset];
}



OperandTypeCategory operand_type_category(int offset)
{
    return __operand_types[(__operand_types.size() - 1) - offset];
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

    __push_operand_impl((void*)(intptr_t)words[0],
                        OperandTypeCategory::primitive_wide);

    __push_operand_impl((void*)(intptr_t)words[1],
                        OperandTypeCategory::primitive_wide);
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

    __operand_types.push_back(
        __operand_types[(__operand_types.size() - 1) - offset]);
}



void dup_x1()
{
    auto v1 = __operand_stack.back();
    __operand_stack.insert(__operand_stack.end() - 2, v1);

    auto tp = __operand_types.back();
    __operand_types.insert(__operand_types.end() - 2, tp);
}



void dup_x2()
{
    // FIXME: would be more efficient to push a copy and swap

    auto v1 = __operand_stack.back();
    __operand_stack.insert(__operand_stack.end() - 3, v1);

    auto tp = __operand_types.back();
    __operand_types.insert(__operand_types.end() - 3, tp);
}



void dup2_x1()
{
    // If the stack top is category 2 and the next element is category 1, this
    // code still works, right? I believe that everything should work just
    // fine...

    //
    // ..., value3, value2, value1 ->
    // ..., value2, value1, value3, value2, value1
    //

    {
        auto v1 = *(__operand_stack.end() - 2);
        __operand_stack.insert(__operand_stack.end() - 3, v1);
    }

    {
        auto v1 = *(__operand_stack.end() - 1);
        __operand_stack.insert(__operand_stack.end() - 3, v1);
    }

    {
        auto v1 = *(__operand_types.end() - 2);
        __operand_types.insert(__operand_types.end() - 3, v1);
    }

    {
        auto v1 = *(__operand_types.end() - 1);
        __operand_types.insert(__operand_types.end() - 3, v1);
    }
}



void dup2_x2()
{
    // From the Java instruction set documentation:
    //
    //     Form 1:
    // ..., value4, value3, value2, value1 ->
    // ..., value2, value1, value4, value3, value2, value1
    //
    // where value1, value2, value3, and value4 are all values of a category 1
    // computational type (§2.11.1).
    //
    //
    // Form 2:
    // ..., value3, value2, value1 ->
    // ..., value1, value3, value2, value1
    //
    // where value1 is a value of a category 2 computational type and value2 and
    // value3 are both values of a category 1 computational type (§2.11.1).
    //
    //
    // Form 3:
    // ..., value3, value2, value1 ->
    // ..., value2, value1, value3, value2, value1
    //
    // where value1 and value2 are both values of a category 1 computational
    // type and value3 is a value of a category 2 computational type (§2.11.1).
    //
    //
    // Form 4:
    // ..., value2, value1 ->
    // ..., value1, value2, value1
    //
    // where value1 and value2 are both values of a category 2 computational
    // type (§2.11.1).
    //


    // The code below was tested, and is confirmed to work, with form 1. Based
    // on how we implemented things, form 2 should also work with the code
    // below. Come to think of it, form 3 should work fine too, and so should
    // form 4. Why does the java instruction set specification describe four
    // different forms? I suppose it would be relevant for implementations that
    // don't split long/double datatypes into two operand stack slots. This
    // implementation of the jvm implements the operand stack as an array of
    // void*, so all category 1 datatypes will fit in a single stack slot, and
    // category 2 datatypes can be divided into two stack slots (even though
    // doing so wastes space in 64bit), and the same code works for 32bit and
    // 64bit. Admittedly, this code won't run on 16bit systems, but what decade
    // are we in, the 1970s? All of the interesting microcontrollers with decent
    // amounts of RAM are 32 bit anyway...
    {
        auto v1 = *(__operand_stack.end() - 2);
        __operand_stack.insert(__operand_stack.end() - 4, v1);
    }

    {
        auto v1 = *(__operand_stack.end() - 1);
        __operand_stack.insert(__operand_stack.end() - 4, v1);
    }

    {
        auto v1 = *(__operand_types.end() - 2);
        __operand_types.insert(__operand_types.end() - 4, v1);
    }

    {
        auto v1 = *(__operand_types.end() - 1);
        __operand_types.insert(__operand_types.end() - 4, v1);
    }
}



template <typename F>
void multi_array_build(Array* parent,
                       int depth,
                       int dim,
                       int dimensions,
                       F&& create_array)
{
    for (int i = 0; i < parent->size_; ++i) {
        if ((dimensions - 2) - depth == 0) {
            auto nested = create_array(dim);
            if (nested == nullptr) {
                unhandled_error("oom");
            }
            memcpy(parent->data() + i * sizeof(Array*), &nested, sizeof(Array*));
        } else {
            Array* nested;
            memcpy(&nested, parent->data() + i * sizeof(Array*), sizeof(nested));
            multi_array_build(nested, depth + 1, dim, dimensions, create_array);
        }
    }
}



// Extract the nested typename from a multidimensional array type descriptor.
Slice multi_nested_typename(Slice type_descriptor)
{
    int i = 0;
    while (type_descriptor.ptr_[i] == '[') {
        ++type_descriptor.ptr_;
        --type_descriptor.length_;
    }

    if (type_descriptor.ptr_[i] == 'L') {
        // Object type descriptors include a trailing semicolon, and a leading
        // L.
        ++type_descriptor.ptr_;
        type_descriptor.length_ -= 2;
    }

    return type_descriptor;
}



void swap()
{
    std::swap(__operand_stack[__operand_stack.size() - 1],
              __operand_stack[__operand_stack.size() - 2]);

    std::swap(__operand_types[__operand_types.size() - 1],
              __operand_types[__operand_types.size() - 2]);
}



void pop_operand()
{
    __operand_stack.pop_back();
    __operand_types.pop_back();
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
        dup_x1          = 0x5a,
        dup_x2          = 0x5b,
        dup2            = 0x5c,
        dup2_x1         = 0x5d,
        dup2_x2         = 0x5e,
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
        multianewarray  = 0xc5,
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
        lshl            = 0x79,
        lshr            = 0x7b,
        lushr           = 0x7d,
        lxor            = 0x83,
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
        tableswitch     = 0xaa,
        lookupswitch    = 0xab,
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
        getstatic       = 0xb2,
        putstatic       = 0xb3,
        getfield        = 0xb4,
        putfield        = 0xb5,
        __goto          = 0xa7,
        __goto_w        = 0xc8,
        invokedynamic   = 0xba,
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



void invoke_static_block(Class* clz);



Class* import_class(Slice classpath, const char* classfile_data)
{
    if (auto clz = parse_classfile(classpath, classfile_data)) {
        invoke_static_block(clz);
        return clz;
    }
    return nullptr;
}



Class* import(Slice classpath)
{
    if (jars == nullptr) {
        unhandled_error("cannot load class with no jars loaded!");
    }

    auto current = jars;
    while (current) {
        auto data = jar::load_classfile(current->jar_file_data_, classpath);
        if (data.length_) {
            return import_class(classpath, data.ptr_);
        }
        current = current->next_;
    }

    return nullptr;
}



void register_class(Slice name, Class* clz)
{
    classtable::insert(name, clz);
}



using Exception = Object;



Exception* execute_bytecode(Class* clz,
                            const u8* bytecode,
                            const ClassFile::ExceptionTable* exception_table);



void pop_arguments(const ArgumentInfo& argc)
{
    for (int i = 0; i < argc.operand_count_; ++i) {
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
        store_local(local_param_index++, self, OperandTypeCategory::object);
        stack_load_index--;
    }

    const char* str = type_signature.ptr_;
    if (str) {
        int i = 1;
        while (true) {
            switch (str[i]) {
            default:
                store_local(local_param_index,
                            load_operand(stack_load_index--),
                            OperandTypeCategory::primitive);
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

            case '[':
                ++i;
                while (str[i] == '[') {
                    ++i;
                }
                if (str[i] not_eq 'L') {
                    // We're a primitive array. Only one character follows the
                    // first one.
                    ++i;
                    store_local(local_param_index,
                                load_operand(stack_load_index--),
                                OperandTypeCategory::object);
                    ++local_param_index;
                    break;
                }
                goto OBJECT; // Intentional fallthrough. We started parsing an
                             // array, only to find that it's an array of
                             // objects. Continue parsing in the next case for
                             // 'L'.


            OBJECT:
            case 'L':
                store_local(local_param_index,
                            load_operand(stack_load_index--),
                            OperandTypeCategory::object);
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
    if (auto entry = classtable::load(class_name)) {
        return entry;
    }

    if (auto clz = import(class_name)) {
        return clz;
    } else {
        unhandled_error("missing class!");
    }
}



Slice classname(Class* current_module, u16 class_index)
{
    auto c_clz =
        (const ClassFile::ConstantClass*)current_module->constants_->load(
            class_index);

    auto cname =
        current_module->constants_->load_string(c_clz->name_index_.get());

    return cname;
}



Class* load_class(Class* current_module, u16 class_index)
{
    return load_class_by_name(classname(current_module, class_index));
}



std::pair<const ClassFile::MethodInfo*, Class*>
lookup_method(Class* clz, Slice lhs_name, Slice lhs_type)
{
    if (auto mtd = clz->load_method(lhs_name, lhs_type)) {
        return {mtd, clz};
    }

    if (clz->super_ == nullptr) {
        puts("method lookup failed");
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

            for (int i = 0; i < argc.operand_count_ + 1; // +1 for self
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
                unhandled_error("failed to load class, TODO: raise error");
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
        puts("missing method");
        return TODO_throw_proper_exception();
    }
}



Exception* invoke_special(Class* clz, u16 method_index)
{
    return dispatch_method(clz, method_index, true, true);
}



Object* make_instance_impl(Class* clz)
{
    const auto instance_size = clz->instance_size();

    // printf("instance size %ld\n", fields_size);

    auto mem = (Object*)heap::allocate(instance_size);
    if (mem == nullptr) {
        unhandled_error("oom");
    }
    new (mem) Object(clz);
    return mem;
}



Object* clone(Object* self)
{
    if (self->class_ == &reference_array_class or
        self->class_ == &primitive_array_class) {

        auto src = (Array*)self;

        auto dst = (Array*)jvm::heap::allocate(src->memory_footprint());
        if (dst == nullptr) {
            unhandled_error("oom");
        }

        memcpy(dst, src, src->memory_footprint());

        return (Object*)dst;

    } else if (self->class_ == &return_address_class) {
        // TODO: can user code ever clone a ReturnAddress? Should be impossible.
        return nullptr;
    } else {
        auto inst = make_instance_impl(self->class_);
        auto size = self->class_->instance_size();

        memcpy(inst, self, size);

        return inst;
    }
    return nullptr;
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
    return make_instance_impl(
        load_class_by_name(Slice::from_c_str("java/lang/Throwable")));
}



Object* make_string(Slice data)
{
    // We want to create a char array, and invoke the String(char[])
    // constructor with the array. In this way, the entire string class can
    // be written in java.

    {
        auto array = Array::create(data.length_, 1, Array::Type::t_char);

        // Copy utf8 string data from classfile to char array.
        memcpy(array->data(), data.ptr_, data.length_);

        // Preserve on stack, in case string instance allocation below
        // triggers the gc. We'll need the array to be on the stack when
        // invoking the string constructor anyway.
        push_operand_a(*(Object*)array);
    }


    auto string_class =
        load_class_by_name(Slice::from_c_str("java/lang/String"));

    push_operand_a(*(Object*)make_instance_impl(string_class));

    // After calling the constructor, we want to leave a copy of the string
    // on the stack.
    dup_x1();

    swap(); // reorder arguments on stack:  ... self, char[] -->

    auto ctor_typeinfo = Slice::from_c_str("([C)V");
    if (auto mtd = string_class->load_method(Slice::from_c_str("<init>"),
                                             ctor_typeinfo)) {
        ArgumentInfo argc;
        argc.argument_count_ = 2;
        argc.operand_count_ = 2;
        auto exn = invoke_method(string_class,
                                 (Object*)load_operand(1),
                                 mtd,
                                 argc,
                                 ctor_typeinfo);
        if (exn) {
            unhandled_error("exception from String(char[])");
        }
    } else {
        unhandled_error("missing String(char[])");
    }

    auto result = load_operand(0);
    pop_operand();

    return (Object*)result;
}



// TODO: This make_exception code isn't used yet, test it...
Exception* make_exception(const char* classpath, const char* error)
{
    push_operand_a(*(Object*)make_string(Slice::from_c_str(error)));
    auto clz = ((Object*)load_operand(0))->class_;
    push_operand_a(*make_instance_impl(load_class_by_name(Slice::from_c_str(classpath))));
    dup_x1();
    swap();

    auto ctor_typeinfo = Slice::from_c_str("(Ljava/lang/String;)V");
    if (auto mtd = clz->load_method(Slice::from_c_str("<init>"),
                                    ctor_typeinfo)) {
        ArgumentInfo argc;
        argc.argument_count_ = 2;
        argc.operand_count_ = 2;
        auto exn = invoke_method(clz,
                                 (Object*)load_operand(1),
                                 mtd,
                                 argc,
                                 ctor_typeinfo);
        if (exn) {
            unhandled_error("exception while constructing exception!?");
        }
    } else {
        unhandled_error("missing constructor");
    }

    auto result = load_operand(0);
    pop_operand();

    return (Object*)result;
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

    case ClassFile::ConstantType::t_string: {
        auto str = (ClassFile::ConstantString*)c;
        auto ustr = clz->constants_->load_string(str->string_index_.get());
        push_operand_a(*(Object*)make_string(ustr));
        break;
    }

    default:
        unhandled_error("unhandled ldc");
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
        unhandled_error("invalid ldc2");
        break;
    }
}



bool primitive_array_type_compare(Array* array, Slice typedescriptor)
{
    if (typedescriptor.length_ == 2) {
        if (typedescriptor.ptr_[0] not_eq '[') {
            return false;
        }

        auto type = array->metadata_.primitive_.type_;

        switch (typedescriptor.ptr_[1]) {
        case 'I':
            return type == Array::Type::t_int;
        case 'F':
            return type == Array::Type::t_float;
        case 'S':
            return type == Array::Type::t_short;
        case 'C':
            return type == Array::Type::t_char;
        case 'J':
            return type == Array::Type::t_long;
        case 'D':
            return type == Array::Type::t_double;
        case 'B':
            return type == Array::Type::t_byte;
        case 'Z':
            return type == Array::Type::t_boolean;
        }
    }
    // TODO: what about other types, like byte?
    return false;
}



bool reference_array_type_compare(Array* array, Slice typedescriptor)
{
    [[maybe_unused]] auto c = array->metadata_.class_type_;

    if (typedescriptor.length_ > 2) {
        // NOTE: The ref array class type desc begins with "[L"
        typedescriptor.ptr_ += 2;
        typedescriptor.length_ -= 2;

        // TODO...

        return true;
    } else {
        return false;
    }
}



bool checkcast(Object* obj, Slice class_name)
{

    if (obj->class_ == &primitive_array_class) {
        return primitive_array_type_compare((Array*)obj, class_name);
    } else if (obj->class_ == &reference_array_class) {
        return reference_array_type_compare((Array*)obj, class_name);
    }

    {
        auto other = load_class_by_name(class_name);

        // First pass: direct inheritance
        auto current = obj->class_;
        while (current) {
            if (current == other) {
                return true;
            }
            current = current->super_;
        }

        // Second pass: check interfaces (slower)
        current = obj->class_;
        while (current) {
            if (auto interfaces = current->interfaces()) {
                auto vals = (network_u16*)((u8*)interfaces +
                                           sizeof(ClassFile::HeaderSection2));
                for (int i = 0; i < interfaces->interfaces_count_.get(); ++i) {
                    if (class_name == classname(current, vals[i].get())) {
                        return true;
                    }
                }
            }
            current = current->super_;
        }
    }

    return false;
}



bool instanceof (Object * obj, Class* clz)
{
    auto current = obj->class_;
    while (current) {
        if (current == clz) {
            return true;
        }
        current = current->super_;
    }

    current = obj->class_;
    while (current) {
        if (auto interfaces = current->interfaces()) {
            auto vals = (network_u16*)((u8*)interfaces +
                                       sizeof(ClassFile::HeaderSection2));
            for (int i = 0; i < interfaces->interfaces_count_.get(); ++i) {
                if (clz ==
                    load_class_by_name(classname(current, vals[i].get()))) {
                    return true;
                }
            }
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



void make_invokedynamic_callsite(Class* clz, int bootstrap_method_index)
{
    auto opt = (Class::OptionBootstrapMethodInfo*)clz->load_option(
        Class::Option::Type::bootstrap_methods);

    if (opt) {
        auto method_info =
            opt->bootstrap_methods_->load(bootstrap_method_index);

        if (method_info) {
            [[maybe_unused]] auto ref =
                (ClassFile::ConstantRef*)clz->constants_->load(
                    method_info->bootstrap_method_ref_.get());

            unhandled_error("TODO: finish implementing invokedynamic");
        }
    }
}



static inline s32 arithmetic_right_shift_32(s32 value, s32 amount)
{
    s32 s = -((u32)value >> 31);
    s32 sar = (s ^ value) >> amount ^ s;
    return sar;
}



static inline s64 arithmetic_right_shift_64(s64 value, s64 amount)
{
    s64 s = -((u64)value >> 63);
    s64 sar = (s ^ value) >> amount ^ s;
    return sar;
}



Exception* execute_bytecode(Class* clz,
                            const u8* bytecode,
                            const ClassFile::ExceptionTable* exception_table)
{
#define JVM_THROW_EXN()                                                        \
    push_operand_a(*(Object*)TODO_throw_proper_exception());                   \
    goto THROW;


    u32 pc = 0;

    while (true) {
        // printf("%d %d\n", pc, bytecode[pc]);
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

            auto c = load_class(clz, ((network_u16*)&bytecode[pc + 1])->get());

            auto array = Array::create(len, c);

            if (array == nullptr) {
                unhandled_error("oom");
            }

            push_operand_a(*(Object*)array);
            pc += 3;
            break;
        }

        case Bytecode::multianewarray: {
            // auto c = load_class(clz, ((network_u16*)&bytecode[pc + 1])->get());
            const u8 dimensions = bytecode[pc + 3];

            if (dimensions == 1) {
                unhandled_error("multianewarray with dimension 1");
                // TODO: branch to the anewarray code.
            }

            const auto len = load_operand_i(dimensions - 1);
            push_operand_a(
                *(Object*)Array::create(len, &reference_array_class));

            Object* result = nullptr;

            {
                auto outer_array = [] { return (Array*)load_operand(0); };

                if (outer_array() == nullptr) {
                    unhandled_error("oom");
                }

                int i = dimensions - 2;

                while (i > -1) {
                    auto dim = load_operand_i(i + 1);

                    if (i == 0) {
                        multi_array_build(
                            outer_array(), i, dim, dimensions, [&](int dim) {
                                auto cname = multi_nested_typename(classname(
                                    clz,
                                    ((network_u16*)&bytecode[pc + 1])->get()));
                                if (cname.length_ == 1) {
                                    Array::Type type;
                                    u8 size;
                                    switch (cname.ptr_[0]) {
                                    case 'F':
                                        size = 4;
                                        type = Array::Type::t_float;
                                        break;

                                    case 'S':
                                        size = 2;
                                        type = Array::Type::t_short;
                                        break;

                                    case 'C':
                                        size = 1;
                                        type = Array::Type::t_char;
                                        break;

                                    case 'J':
                                        size = 8;
                                        type = Array::Type::t_long;
                                        break;

                                    case 'D':
                                        size = 8;
                                        type = Array::Type::t_double;
                                        break;

                                    case 'B':
                                        size = 1;
                                        type = Array::Type::t_byte;
                                        break;

                                    case 'Z':
                                        size = 1;
                                        type = Array::Type::t_boolean;
                                        break;

                                    case 'I':
                                        size = 4;
                                        type = Array::Type::t_int;
                                        break;

                                    default:
                                        unhandled_error("unsupported primitive "
                                                        "in multi array");
                                    }
                                    return Array::create(dim, size, type);

                                } else {
                                    return Array::create(
                                        dim, load_class_by_name(cname));
                                }
                            });
                    } else {
                        multi_array_build(
                            outer_array(), i, dim, dimensions, [](int dim) {
                                return Array::create(dim,
                                                     &reference_array_class);
                            });
                    }

                    --i;
                }

                // Ok, so now we want to actually fill the array with arrays.


                result = (Object*)outer_array();
                pop_operand();
            }

            for (int i = 0; i < dimensions; ++i) {
                pop_operand();
            }

            if (result) {
                push_operand_a(*result);
            } else {
                unhandled_error("oom?");
            }

            pc += 4;
            break;
        }

        case Bytecode::newarray: {
            const int element_count = load_operand_i(0);
            pop_operand();

            int element_size = 4;
            switch (bytecode[pc + 1]) {
            case Array::Type::t_boolean:
            case Array::Type::t_char:
            case Array::Type::t_byte:
                element_size = 1;
                break;

            case Array::Type::t_float:
            case Array::Type::t_int:
                element_size = 4;
                break;

            case Array::Type::t_short:
                element_size = 2;
                break;

            case Array::Type::t_double:
            case Array::Type::t_long:
                element_size = 8;
                break;
            }

            auto array = Array::create(
                element_count, element_size, (Array::Type)bytecode[pc + 1]);

            push_operand_a(*(Object*)array);
            pc += 2;
            break;
        }

        case Bytecode::arraylength: {
            auto array = (Array*)load_operand(0);
            pop_operand();

            if (array == nullptr) {
                unhandled_error("nullptr exception !");
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
                unhandled_error("nullptr exception, ...");
            }

            if (array->check_bounds(index)) {
                Object* result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand_a(*result);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            pop_operand();
            pop_operand();
            pop_operand();

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                JVM_THROW_EXN();
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
            auto obj = (Object*)load_operand(0);

            if (obj == nullptr) {
                // According to the JVM specification, checkcast for null is a
                // no-op.
                pc += 3;
                break;
            }

            pop_operand();

            auto cname =
                classname(clz, ((network_u16*)&bytecode[pc + 1])->get());

            if (checkcast(obj, cname)) {
                push_operand_a(*obj);
                pc += 3;
            } else {
                JVM_THROW_EXN();
            }
            break;
        }

        case Bytecode:: instanceof: {
            auto obj = (Object*)load_operand(0);
            pop_operand();

            if (obj == nullptr) {
                push_operand_i(0);
                pc += 3;
                break;
            }

            auto cname =
                classname(clz, ((network_u16*)&bytecode[pc + 1])->get());

            if (obj->class_ == &primitive_array_class) {
                push_operand_i(
                    primitive_array_type_compare((Array*)obj, cname));
            } else if (obj->class_ == &reference_array_class) {
                push_operand_i(
                    reference_array_type_compare((Array*)obj, cname));
            } else {
                auto other = load_class_by_name(cname);
                push_operand_i(instanceof (obj, other));
            }

            pc += 3;

            break;
        }

        case Bytecode::dup:
            dup(0);
            ++pc;
            break;

        case Bytecode::dup_x1:
            dup_x1();
            ++pc;
            break;

        case Bytecode::dup_x2:
            dup_x2();
            ++pc;
            break;

        case Bytecode::dup2_x1:
            dup2_x1();
            ++pc;
            break;

        case Bytecode::dup2_x2:
            dup2_x2();
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
                JVM_THROW_EXN();
            }

            auto c =
                clz->constants_->load(((network_u16*)&bytecode[pc + 1])->get());

            auto sub = (SubstitutionField*)c;

            u8* obj_ram = arg->data();

            if (sub->object_) {
                Object* val;
                memcpy(&val, obj_ram + sub->offset_, sizeof val);
                push_operand_a(*val);
            } else {
                switch (1 << sub->size_) {
                case 1:
                    push_operand_i(obj_ram[sub->offset_]);
                    break;

                case 2: {
                    s16 val;
                    memcpy(&val, obj_ram + sub->offset_, 2);
                    push_operand_i(val);
                    break;
                }

                case 4: {
                    s32 val;
                    memcpy(&val, obj_ram + sub->offset_, 4);
                    push_operand_i(val);
                    break;
                }

                case 8: {
                    s64 val;
                    memcpy(&val, obj_ram + sub->offset_, 8);
                    push_wide_operand_l(val);
                }
                }
            }
            pc += 3;
            break;
        }

        case Bytecode::putfield: {
            if (operand_type_category(0) ==
                OperandTypeCategory::primitive_wide) {

                // NOTE: I haven't tested this code for writing long/double,
                // hopefully it actually works. TODO: unit tests...

                auto obj = (Object*)load_operand(2);
                auto value = load_wide_operand_l(0);

                pop_operand();
                pop_operand();
                pop_operand();

                if (obj == nullptr) {
                    JVM_THROW_EXN();
                }

                auto c = clz->constants_->load(
                    ((network_u16*)&bytecode[pc + 1])->get());

                auto sub = (SubstitutionField*)c;

                u8* obj_ram = obj->data();

                if (sub->size_ not_eq SubstitutionField::Size::b8) {
                    JVM_THROW_EXN();
                }

                memcpy(obj_ram + sub->offset_, &value, sizeof(value));

            } else {
                auto obj = (Object*)load_operand(1);
                auto value = load_operand(0);

                pop_operand();
                pop_operand();

                if (obj == nullptr) {
                    JVM_THROW_EXN();
                }

                auto c = clz->constants_->load(
                    ((network_u16*)&bytecode[pc + 1])->get());

                auto sub = (SubstitutionField*)c;

                u8* obj_ram = obj->data();

                if (sub->object_) {
                    auto obj_value = (Object*)value;
                    memcpy(obj_ram + sub->offset_, &obj_value, sizeof value);
                } else {
                    switch (1 << sub->size_) {
                    case 1:
                        obj_ram[sub->offset_] = (u8)(intptr_t)value;
                        break;

                    case 2: {
                        s16 val = (s16)(intptr_t)value;
                        memcpy(obj_ram + sub->offset_, &val, sizeof val);
                        break;
                    }

                    case 4: {
                        s32 val = (s32)(intptr_t)value;
                        memcpy(obj_ram + sub->offset_, &val, sizeof val);
                        break;
                    }

                    case 8: {
                        JVM_THROW_EXN();
                    }
                    }
                }
            }
            pc += 3;
            break;
        }

        case Bytecode::getstatic: {
            auto index = ((network_u16*)&bytecode[pc + 1])->get();
            if (auto opt = clz->lookup_static(index)) {
                if (opt->is_object_) {
                    Object* obj;
                    memcpy(&obj, opt->data(), sizeof(Object*));
                    push_operand_a(*obj);
                } else {
                    switch (opt->field_size_) {
                    case 1: {
                        u8 val = *opt->data();
                        push_operand_i(val);
                        break;
                    }
                    case 2: {
                        s16 val;
                        memcpy(&val, opt->data(), 2);
                        push_operand_i(val);
                        break;
                    }
                    case 4: {
                        s32 val;
                        memcpy(&val, opt->data(), 4);
                        push_operand_i(val);
                        break;
                    }
                    case 8: {
                        s64 val;
                        memcpy(&val, opt->data(), 8);
                        push_wide_operand_l(val);
                        break;
                    }
                    }
                }
            } else {
                unhandled_error("critical error in getstatic");
            }
            pc += 3;
            break;
        }

        case Bytecode::putstatic: {
            auto index = ((network_u16*)&bytecode[pc + 1])->get();
            if (auto opt = clz->lookup_static(index)) {
                if (opt->is_object_) {
                    auto val = load_operand(0);
                    pop_operand();
                    memcpy(opt->data(), &val, sizeof val);
                } else {
                    switch (opt->field_size_) {
                    case 1: {
                        *opt->data() = (u8)load_operand_i(0);
                        pop_operand();
                        break;
                    }
                    case 2: {
                        s16 val = load_operand_i(0);
                        pop_operand();
                        memcpy(opt->data(), &val, 2);
                        break;
                    }
                    case 4: {
                        s32 val = load_operand_i(0);
                        pop_operand();
                        memcpy(opt->data(), &val, 4);
                        break;
                    }
                    case 8: {
                        s64 val = load_wide_operand_l(0);
                        pop_operand();
                        pop_operand();
                        memcpy(opt->data(), &val, 8);
                        break;
                    }
                    }
                }
            } else {
                unhandled_error("critical error in putstatic");
            }
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
                JVM_THROW_EXN();
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

        // FIXME: ishl and ishr may not be implemented correctly (sign extension
        // and negative numbers).
        case Bytecode::ishl: {
            const s32 result = load_operand_i(1) << (load_operand_i(0) & 0x1f);
            pop_operand();
            pop_operand();
            push_operand_i(result);
            ++pc;
            break;
        }

        case Bytecode::ishr: {
            const s32 result = arithmetic_right_shift_32(
                load_operand_i(1), load_operand_i(0) & 0x1f);
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
            if (load_operand_i(0) <= load_operand_i(1)) {
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

        case Bytecode::lookupswitch: {
            auto key = load_operand_i(0);
            pop_operand();

            int i = pc + 1;

            if (i % 4 not_eq 0) {
                // skip padding bytes
                i += 4 - i % 4;
            }

            auto default_br = ((network_s32*)(bytecode + i))->get();
            i += sizeof(network_s32);
            auto npairs = ((network_s32*)(bytecode + i))->get();
            i += sizeof(network_s32);

            struct MatchPair {
                network_s32 case_;
                network_s32 branch_;
            };

            auto pairs = (const MatchPair*)(bytecode + i);

            for (int i = 0; i < npairs; ++i) {
                auto& pair = pairs[i];
                if (pair.case_.get() == key) {
                    pc += pair.branch_.get();
                    goto DONE;
                }
            }

            pc += default_br;

        DONE:
            break;
        }

        case Bytecode::tableswitch: {
            auto index = load_operand_i(0);
            pop_operand();

            int i = pc + 1;

            if (i % 4 not_eq 0) {
                // skip padding bytes
                i += 4 - i % 4;
            }

            auto default_br = ((network_s32*)(bytecode + i))->get();
            i += sizeof(network_s32);
            auto low = ((network_s32*)(bytecode + i))->get();
            i += sizeof(network_s32);
            auto high = ((network_s32*)(bytecode + i))->get();

            if (index < low or index > high) {
                pc += default_br;
            } else {
                auto table_offset = index - low;
                i += sizeof(network_s32);
                auto jump_table = (network_s32*)(bytecode + i);
                pc += jump_table[table_offset].get();
            }
            break;
        }

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
            break;
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                double result;
                memcpy(&result, array->address(index), sizeof result);
                push_wide_operand_d(result);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                s64 result;
                memcpy(&result, array->address(index), sizeof result);
                push_wide_operand_l(result);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                s16 result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand_i(result);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                JVM_THROW_EXN();
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
            store_local(
                bytecode[pc + 1], load_operand(0), OperandTypeCategory::object);
            pop_operand();
            pc += 2;
            break;

        case Bytecode::astore_0:
            store_local(0, load_operand(0), OperandTypeCategory::object);
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_1:
            store_local(1, load_operand(0), OperandTypeCategory::object);
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_2:
            store_local(2, load_operand(0), OperandTypeCategory::object);
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_3:
            store_local(3, load_operand(0), OperandTypeCategory::object);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore:
        case Bytecode::istore:
            store_local(bytecode[pc + 1],
                        load_operand(0),
                        OperandTypeCategory::primitive);
            pop_operand();
            pc += 2;
            break;

        case Bytecode::fstore_0:
        case Bytecode::istore_0:
            store_local(0, load_operand(0), OperandTypeCategory::primitive);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_1:
        case Bytecode::istore_1:
            store_local(1, load_operand(0), OperandTypeCategory::primitive);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_2:
        case Bytecode::istore_2:
            store_local(2, load_operand(0), OperandTypeCategory::primitive);
            pop_operand();
            ++pc;
            break;

        case Bytecode::fstore_3:
        case Bytecode::istore_3:
            store_local(3, load_operand(0), OperandTypeCategory::primitive);
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
                        OperandTypeCategory::primitive);
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                *array->address(index) = value;
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                *array->address(index) = value;
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                u8 value = *array->address(index);
                push_operand_i(value);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                s8 value = *array->address(index);
                push_operand_i(value);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                memcpy(array->address(index), &value, sizeof value);
            } else {
                JVM_THROW_EXN();
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
                unhandled_error("nullptr exception");
            }

            if (array->check_bounds(index)) {
                s32 result;
                memcpy(&result, array->address(index), sizeof result);
                push_operand_i(result);
            } else {
                JVM_THROW_EXN();
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
                JVM_THROW_EXN();
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

        case Bytecode::lxor: {
            auto lhs = load_wide_operand_l(2);
            auto rhs = load_wide_operand_l(0);
            pop_operand();
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs ^ rhs);
            ++pc;
            break;
        }

        case Bytecode::lushr: {
            auto lhs = load_wide_operand_l(1);
            auto rhs = load_operand_i(0);
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(lhs >> rhs);
            ++pc;
            break;
        }

        case Bytecode::lshl: {
            // FIXME: Potentially non-portable?
            const auto result = load_wide_operand_l(1)
                                << (load_operand_i(0) & 0x1f);
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(result);
            ++pc;
            break;
        }

        case Bytecode::lshr: {
            const auto result = arithmetic_right_shift_64(
                load_wide_operand_l(1), load_operand_i(0) & 0x1f);
            pop_operand();
            pop_operand();
            pop_operand();
            push_wide_operand_l(result);
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

        case Bytecode::invokedynamic: {
            auto info =
                (const ClassFile::ConstantInvokeDynamic*)clz->constants_->load(
                    ((network_u16*)(bytecode + pc + 1))->get());

            make_invokedynamic_callsite(
                clz, info->bootstrap_method_attr_index_.get());

            // TODO...

            pc += 5;
            break;
        }

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



#ifndef PROJECT_ROOT
#define PROJECT_ROOT ""
#endif



INCBIN(lang_jar, PROJECT_ROOT "src/Lang.jar");



void invoke_static_block(Class* clz)
{
    const auto name = Slice::from_c_str("<clinit>");
    const auto signature = Slice::from_c_str("()V");

    if (auto mtd = clz->load_method(name, signature)) {
        ArgumentInfo argc;
        auto exn = invoke_method(clz, nullptr, mtd, argc, signature);
        if (exn) {
            // TODO: what to do! I'm not really sure where we should propagate
            // an exception from a static block...
            unhandled_error("exception from static block");
        }
    }
}



void bootstrap()
{
    bind_jar((const char*)lang_jar_data);

    if (auto obj_class = import(Slice::from_c_str("java/lang/Object"))) {
        obj_class->super_ = nullptr;

        primitive_array_class.super_ = obj_class;
        reference_array_class.super_ = obj_class;
        return_address_class.super_ = obj_class;

        jni::bind_native_method(obj_class,
                                Slice::from_c_str("clone"),
                                Slice::from_c_str("TODO_:)"),
                                [] {
                                    auto self = (Object*)load_local(0);
                                    push_operand_a(*clone(self));
                                });

        // Needs to be deferred until we've completed the above steps.
        invoke_static_block(obj_class);
    } else {
        unhandled_error("failed to load object class");
    }

    if (import(Slice::from_c_str("java/lang/String"))) {
        // ...
    } else {
        unhandled_error("failed to load string class");
    }

    if (auto runtime_class = import(Slice::from_c_str("java/lang/Runtime"))) {

        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("exit"),
                                Slice::from_c_str("TODO_:)"),
                                [] { exit((intptr_t)load_local(1)); });


        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("gc"),
                                Slice::from_c_str("TODO_:)"),
                                [] { java::jvm::gc::collect(); });

        jni::bind_native_method(
            runtime_class,
            Slice::from_c_str("totalMemory"),
            Slice::from_c_str("TODO_:)"),
            [] { push_wide_operand_l(jvm::heap::total()); });

        jni::bind_native_method(runtime_class,
                                Slice::from_c_str("freeMemory"),
                                Slice::from_c_str("TODO_:)"),
                                [] {
                                    push_wide_operand_l(jvm::heap::total() -
                                                        jvm::heap::used());
                                });
    }

    if (import(Slice::from_c_str("java/lang/Throwable"))) {
        // ...
    }
}



int start(Class* entry_point)
{
    const auto type_signature = Slice::from_c_str("([Ljava/lang/String;)V");


    if (auto entry = entry_point->load_method(Slice::from_c_str("main"),
                                              type_signature)) {

        push_operand_p(nullptr); // String[] args

        ArgumentInfo argc;
        argc.argument_count_ = 1;
        argc.operand_count_ = 1;

        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        auto exn = java::jvm::invoke_method(
            entry_point, nullptr, entry, argc, type_signature);
        auto end = high_resolution_clock::now();
        std::cout
            << std::chrono::duration_cast<nanoseconds>(end - start).count()
            << std::endl;

        heap::print_stats([](const char* str) { printf("%s", str); });

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
    bootstrap();

    bind_jar(jar_file_bytes);

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
        invoke_static_block(clz);
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
        return java::jvm::start_from_jar(str.c_str(), classpath);
    } else {
        return java::jvm::start_from_classfile(str.c_str(), classpath);
    }

    return 0;
}
