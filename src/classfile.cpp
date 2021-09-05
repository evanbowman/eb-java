#include "classfile.hpp"
#include "class.hpp"
#include "memory.hpp"
#include "vm.hpp"
#include <iostream>



const char* get_file_contents(const char* file);



namespace java {



std::pair<SubstitutionField::Size, bool> get_field_size(Slice field_type)
{
    SubstitutionField::Size field_size = SubstitutionField::b4;

    bool is_object = false;

    // Add to byte offset based on size of the typeinfo in the type
    // descriptor.
    if (field_type == Slice::from_c_str("I")) {
        field_size = SubstitutionField::b4;
    } else if (field_type == Slice::from_c_str("F")) {
        field_size = SubstitutionField::b4;
    } else if (field_type == Slice::from_c_str("S")) {
        field_size = SubstitutionField::b2;
    } else if (field_type == Slice::from_c_str("C")) {
        field_size = SubstitutionField::b1;
    } else if (field_type == Slice::from_c_str("J")) {
        field_size = SubstitutionField::b8;
    } else if (field_type == Slice::from_c_str("D")) {
        field_size = SubstitutionField::b8;
    } else if (field_type.ptr_[0] == 'L' or /* class instance */
               field_type.ptr_[0] == '['    /* array */) {
        // We use a special size enumeration for objects, not
        // because we cannot figure out how large a pointer is on
        // the target architecture, but becauese we care whether a
        // field is an object or a primitive when pushing fields
        // onto the operand stack.
        if (sizeof(Object*) == 4) {
            field_size = SubstitutionField::b4;
        } else if (sizeof(Object*) == 8) {
            field_size = SubstitutionField::b8;
        } else {
            jvm::unhandled_error("unsupported architecture");
        }

        is_object = true;

    } else {
        jvm::unhandled_error("invalid field size");
    }
    return {field_size, is_object};
}



// A quite complicated function call. It looks up a class, and determines the
// byte offset of a field within an instance of that class.
//
// FIXME: this code does not look at access permissions on field access, we
// could potentially have a shadowing bug, if there are private/public vars with
// the same name.
SubstitutionField link_field(Class* current, const ClassFile::ConstantRef& ref)
{
    auto src_nt =
        (const ClassFile::ConstantNameAndType*)current->constants_->load(
            ref.name_and_type_index_.get());

    auto local_field_type =
        current->constants_->load_string(src_nt->descriptor_index_.get());

    auto local_field_name =
        current->constants_->load_string(src_nt->name_index_.get());

    // std::cout << "linking field of name "
    //           << std::string(local_field_name.ptr_, local_field_name.length_)
    //           << ", type "
    //           << std::string(local_field_type.ptr_, local_field_type.length_)
    //           << std::endl;

    // Ok, so we're given a refrence to a field in an arbitrary class. To
    // determine the byte offset of the field within an instance of said class,
    // we begin by loading the class itself.
    if (auto clz = jvm::load_class(current, ref.class_index_.get())) {

        // Storing a flag to indicate whether a fieldref in the constant pool
        // belongs to the class defined in the classfile helps us later when
        // we're running the gc.
        const bool is_local = clz == current;

        // To make things even more complicated, we need to run this thing in a
        // loop, to match inherited fields.
        while (clz) {

            // The byte offset into the instance, where the field is stored.
            u16 instance_offset = 0;

            if (clz->super_) {
                // Ask the parent class how many bytes it requires for its own
                // fields.
                // NOTE: The VM assumes that the parent class was loaded
                // first. Otherwise none of this works!
                instance_offset = clz->super_->instance_fields_size();
            }

            // Now comes the tricky part. Fields are laid out in an object according
            // to the order in which they appear in the fields section of the
            // classfile. So we need to jump to the correct portion of the class
            // file, and build up an offset while iterating over all of the fields.

            auto classfile = clz->classfile_data_;

            // We cannot simply jump directly to the section of the classfile that
            // lists all the field info, we need to iterate over some
            // variable-length structures first.

            auto h1 = (ClassFile::HeaderSection1*)classfile;
            classfile += sizeof(ClassFile::HeaderSection1);

            for (int i = 0; i < h1->constant_count_.get() - 1; ++i) {
                auto c = (const ClassFile::ConstantHeader*)classfile;
                if (c->tag_ == ClassFile::t_double or
                    c->tag_ == ClassFile::t_long) {
                    // Not sure who's bright idea this was... wish I could get
                    // into a time machine and ask the people at Sun about
                    // this...
                    ++i;
                }
                classfile += ClassFile::constant_size(c);
            }

            // Ok, now we've skipped over the first part of the classfile (the
            // constant pool). Next we need to jump across another section, along
            // with the interfaces...

            auto h2 = (ClassFile::HeaderSection2*)classfile;
            classfile += sizeof(ClassFile::HeaderSection2);

            if (h2->interfaces_count_.get()) {
                for (int i = 0; i < h2->interfaces_count_.get(); ++i) {
                    // Each interface is merely an index into the constant pool.
                    classfile += sizeof(u16);
                }
            }

            auto h3 = (ClassFile::HeaderSection3*)classfile;
            classfile += sizeof(ClassFile::HeaderSection3);

            for (int i = 0; i < h3->fields_count_.get(); ++i) {
                auto field = (const ClassFile::FieldInfo*)classfile;
                classfile += sizeof(ClassFile::FieldInfo);

                if (field->access_flags_.get() & 0x08) {
                    // Skip field, the static field does not contribute to the
                    // size of an instance.
                    for (int i = 0; i < field->attributes_count_.get(); ++i) {
                        auto attr = (ClassFile::AttributeInfo*)classfile;
                        classfile += sizeof(ClassFile::AttributeInfo) +
                                     attr->attribute_length_.get();
                    }

                    continue;
                }

                auto field_type = clz->constants_->load_string(
                    field->descriptor_index_.get());

                auto field_name =
                    clz->constants_->load_string(field->name_index_.get());

                auto field_size = get_field_size(field_type);

                SubstitutionField sub(
                    field_size.first, instance_offset, field_size.second);

                if (field_type == local_field_type and
                    field_name == local_field_name) {
                    // NOTE: Do we ever need to check access protections here?
                    // The java compiler will not allow a derived class to
                    // access a base class' public field of the same name if the
                    // derived class includes a private field that shadows the
                    // public one. Methods, in addition, cannot be more private
                    // than ones that they override. But, still, I'm not
                    // entirely confident that I'm not missing
                    // something... while the java compiler checks access rules,
                    // what do other languages that target the java platform do?
                    // i.e. will this jvm incorrectly handle classfiles
                    // generated by Scale/Clojure/Kotlin, by assuming access
                    // controls will be checked in advance by a compiler? TODO:
                    // verify this.

                    sub.local_ = is_local;
                    // std::cout << "found field at offset " << instance_offset << std::endl;
                    return sub;
                }

                instance_offset += sub.real_size();

                // Skip over attributes...
                for (int i = 0; i < field->attributes_count_.get(); ++i) {
                    auto attr = (ClassFile::AttributeInfo*)classfile;
                    classfile += sizeof(ClassFile::AttributeInfo) +
                                 attr->attribute_length_.get();
                }
            }
            clz = clz->super_;
        }
    }

    puts("failed to link field"); // TODO: not a real error for static fields
    return {};
}



void make_static_variable(Class* clz, const ClassFile::FieldInfo* field)
{
    auto field_type =
        clz->constants_->load_string(field->descriptor_index_.get());

    auto field_name = clz->constants_->load_string(field->name_index_.get());

    auto field_size = get_field_size(field_type);
    int size = 1 << field_size.first;
    bool is_object = field_size.second;

    auto opt =
        jvm::classmemory::allocate(sizeof(Class::OptionStaticField) + size,
                                   alignof(Class::OptionStaticField));

    new (opt) Class::OptionStaticField(field_name, (u8)size, is_object);

    clz->append_option((Class::OptionStaticField*)opt);
}



const char* parse_classfile_fields(const char* str,
                                   Class* clz,
                                   const ClassFile::HeaderSection1& h1)
{
    int nfields = 0;
    {
        const char* str =
            ((const char*)&h1) + sizeof(ClassFile::HeaderSection1);

        for (int i = 0; i < h1.constant_count_.get() - 1; ++i) {
            auto c = (const ClassFile::ConstantHeader*)str;
            if (c->tag_ == ClassFile::t_field_ref) {
                ++nfields;
            } else if (c->tag_ == ClassFile::t_double or
                       c->tag_ == ClassFile::t_long) {
                ++i;
            }
            str +=
                ClassFile::constant_size((const ClassFile::ConstantHeader*)str);
        }
    }

    // FIXME: This code reserves extra unused slots for static variables,
    // because the code doesn't know at this point about the fields' access
    // settings.
    clz->constants_->reserve_fields(nfields);

    {
        // Substitute each field ref in this module with a specific byte offset
        // into the class instance.

        const char* str =
            ((const char*)&h1) + sizeof(ClassFile::HeaderSection1);

        for (int i = 0; i < h1.constant_count_.get() - 1; ++i) {
            auto hdr = (const ClassFile::ConstantHeader*)str;
            if (hdr->tag_ == ClassFile::t_field_ref) {
                auto& ref = *(const ClassFile::ConstantRef*)hdr;

                auto field = link_field(clz, ref);

                if (field.valid_) {

                    if (field.offset_ > 2047) {
                        jvm::unhandled_error("field offset exceeds maximum");
                    }

                    clz->constants_->bind_field(i + 1, field);

                    if (auto other_clz =
                            jvm::load_class(clz, ref.class_index_.get())) {
                        // If we're registering a field for the same class as we're
                        // parsing...
                        if (other_clz == clz) {
                            if (clz->cpool_highest_field_ not_eq -1) {
                                auto prev =
                                    (SubstitutionField*)clz->constants_->load(
                                        clz->cpool_highest_field_);
                                if (field.offset_ > prev->offset_) {
                                    clz->cpool_highest_field_ = i + 1;
                                }
                            } else {
                                clz->cpool_highest_field_ = i + 1;
                            }
                        }
                    }
                }
            } else if (hdr->tag_ == ClassFile::t_double or
                       hdr->tag_ == ClassFile::t_long) {
                ++i;
            }

            str +=
                ClassFile::constant_size((const ClassFile::ConstantHeader*)str);
        }
    }


    // Skip over the rest of the file...
    auto h3 = reinterpret_cast<const ClassFile::HeaderSection3*>(str);
    str += sizeof(ClassFile::HeaderSection3);

    for (int i = 0; i < h3->fields_count_.get(); ++i) {
        auto field = (const ClassFile::FieldInfo*)str;
        str += sizeof(ClassFile::FieldInfo);

        if (field->access_flags_.get() & 0x08) {
            make_static_variable(clz, field);
        }

        for (int i = 0; i < field->attributes_count_.get(); ++i) {
            auto attr = (ClassFile::AttributeInfo*)str;
            str += sizeof(ClassFile::AttributeInfo) +
                   attr->attribute_length_.get();
        }
    }

    return str;
}



Class* parse_classfile(Slice classname, const char* str)
{
    auto h1 = reinterpret_cast<const ClassFile::HeaderSection1*>(str);

    if (h1->magic_.get() not_eq 0xcafebabe) {
        return nullptr;
    }

    auto clz = jvm::classmemory::allocate<Class>();

    if (not clz) {
        jvm::unhandled_error("failed to alloc classmemory");
    }

    new (clz) Class();

    clz->classfile_data_ = str;

    str += sizeof(ClassFile::HeaderSection1);


    clz->constants_ = jvm::classmemory::allocate<ConstantPoolCompactImpl>();
    str = clz->constants_->parse(*h1);

    auto h2 = reinterpret_cast<const ClassFile::HeaderSection2*>(str);
    str += sizeof(ClassFile::HeaderSection2);

    if (h2->interfaces_count_.get()) {
        for (int i = 0; i < h2->interfaces_count_.get(); ++i) {
            str += sizeof(u16);
            clz->flags_ |= Class::Flag::implements_interfaces;
        }
    }
    jvm::register_class(classname, clz);


    if (not(classname == Slice::from_c_str("java/lang/Object"))) {
        clz->super_ = jvm::load_class(clz, h2->super_class_.get());
    }


    str = parse_classfile_fields(str, clz, *h1);

    auto h4 = reinterpret_cast<const ClassFile::HeaderSection4*>(str);
    str += sizeof(ClassFile::HeaderSection4);

    clz->methods_ = (void*)h4; // Argh, I know. The class may or may not point
                               // to constant data though, depending on whether
                               // it has a method table, or whether it's running
                               // directly off of the classfile.

    if (h4->methods_count_.get()) {

        for (int i = 0; i < h4->methods_count_.get(); ++i) {
            auto method = (ClassFile::MethodInfo*)str;
            str += sizeof(ClassFile::MethodInfo);

            for (int i = 0; i < method->attributes_count_.get(); ++i) {
                auto attr = (ClassFile::AttributeInfo*)str;
                str += sizeof(ClassFile::AttributeInfo) +
                       attr->attribute_length_.get();
            }
        }
    }

    auto h5 = reinterpret_cast<const ClassFile::HeaderSection5*>(str);
    str += sizeof(ClassFile::HeaderSection5);

    if (h5->attributes_count_.get()) {
        for (int i = 0; i < h5->attributes_count_.get(); ++i) {
            auto attr = (ClassFile::AttributeInfo*)str;
            str += sizeof(ClassFile::AttributeInfo) +
                   attr->attribute_length_.get();

            auto attr_name =
                clz->constants_->load_string(attr->attribute_name_index_.get());

            if (attr_name == Slice::from_c_str("BootstrapMethods")) {
                auto opt = jvm::classmemory::allocate<
                    Class::OptionBootstrapMethodInfo>();

                opt->bootstrap_methods_ =
                    (const ClassFile::BootstrapMethodsAttribute*)attr;

                clz->append_option(opt);
            }
        }
    }

    return clz;
}



} // namespace java
