#pragma once

#include "classfile.hpp"
#include "constantPool.hpp"
#include "slice.hpp"
#include "substitutionField.hpp"



namespace java {



struct Object;



struct Class {

    // We use the Option class for attaching Optional features to a class. Many
    // classes will not need any extra data.
    struct Option {
        Option* next_ = nullptr;
        enum Type {
            null,
            bootstrap_methods,
            static_field,
        } type_ = Type::null;
    };

    template <Option::Type type> struct OptionHeader {
        Option option_;

        OptionHeader()
        {
            option_.type_ = type;
        }
    };


    enum Flag {
        // clang-format off
        __reserved_0__        = (1 << 0),
        has_method_table      = (1 << 1),
        __reserved_1__        = (1 << 2),
        implements_interfaces = (1 << 3),
        // clang-format on
    };

    u16 flags_ = 0;

    // We keep track of the highest index of SubstitutionField in our class
    // instance. In doing so, we know how large class instances need to be.
    s16 cpool_highest_field_ = -1;


    ConstantPool* constants_ = nullptr;

    // methods_ may point to the section of the classfile with the method
    // implementations, if the has_method_table flag is not set, otherwise,
    // methods_ will contain a pointer to a MethodTable objects.
    void* methods_ = nullptr;

    // Assigned if the class has a superclass. All classes have superclasses in
    // our implementation except for java/lang/Object.
    Class* super_ = nullptr;

    // Points to the start of the classfile.
    const char* classfile_data_ = nullptr;


    // TODO: Many classes will not require any options. We can save some space
    // by pre-processing the classfile, and allocating a smaller class if we
    // don't actually need any Optional extensions.
    Option* options_ = nullptr;


    void
    visit_methods(void (*visitor)(Class*, const ClassFile::MethodInfo*, void*),
                  void* arg);


    template <typename T> void append_option(T* option)
    {
        option->header_.option_.next_ = options_;
        options_ = &option->header_.option_;
    }


    Option* load_option(Option::Type type)
    {
        auto current = options_;

        while (current) {
            if (current->type_ == type) {
                return current;
            }

            current = current->next_;
        }

        return nullptr;
    }


    const ClassFile::HeaderSection2* interfaces() const;


    const ClassFile::MethodInfo* load_method(Slice method_name,
                                             Slice type_signature);


    // The extra memory required to hold all fields of an instance of this
    // class. A size of an instance of this class equals sizeof(Object) +
    // class->instance_fields_size().
    size_t instance_fields_size();


    size_t instance_size();


    struct OptionBootstrapMethodInfo {
        OptionHeader<Option::Type::bootstrap_methods> header_;
        const ClassFile::BootstrapMethodsAttribute* bootstrap_methods_;
    };

    struct OptionStaticField {
        OptionHeader<Option::Type::static_field> header_;
        const Slice name_;
        const u8 field_size_ : 7;
        const u8 is_object_ : 1;

        OptionStaticField(Slice name, u8 field_size, bool is_object)
            : name_(name), field_size_(field_size), is_object_(is_object)
        {
        }

        u8* data()
        {
            return ((u8*)this) + sizeof(OptionStaticField);
        }
    };


    OptionStaticField* lookup_static(u16 ref);


    OptionStaticField* lookup_static(Slice field_name)
    {
        auto current = options_;

        while (current) {
            if (current->type_ == Option::Type::static_field) {
                auto field = (OptionStaticField*)current;
                if (field->name_ == field_name) {
                    return field;
                }
            }

            current = current->next_;
        }

        return nullptr;
    }


    // Required for debuggers
    const ClassFile::LineNumberTableAttribute*
    get_line_number_table(const ClassFile::MethodInfo* mtd);
};



} // namespace java
