#include "classfile.hpp"
#include "class.hpp"
#include "vm.hpp"
#include <iostream>



const char* get_file_contents(const char* file);



namespace java {



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
                    puts("TODO: implement static fields");
                    while (true)
                        ;
                }

                auto field_type = clz->constants_->load_string(
                    field->descriptor_index_.get());

                auto field_name =
                    clz->constants_->load_string(field->name_index_.get());

                SubstitutionField::Size field_size = SubstitutionField::b4;

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
                    puts("TODO: implement long fields");
                    while (true)
                        ;
                } else if (field_type == Slice::from_c_str("D")) {
                    puts("TODO: implement doule fields");
                    while (true)
                        ;
                } else if (field_type.ptr_[0] == 'L') {
                    // We use a special size enumeration for objects, not
                    // because we cannot figure out how large a pointer is on
                    // the target architecture, but becauese we care whether a
                    // field is an object or a primitive when pushing fields
                    // onto the operand stack.
                    field_size = SubstitutionField::b_ref;
                } else {
                    std::cout
                        << std::string(field_type.ptr_, field_type.length_)
                        << std::endl;
                    puts("TODO: field sizes...");
                    while (true)
                        ;
                }

                if (field_type == local_field_type and
                    field_name == local_field_name) {
                    // std::cout << "found field at offset " << instance_offset << std::endl;
                    return {field_size, instance_offset};
                }

                if (field_size == SubstitutionField::b_ref) {
                    instance_offset += sizeof(Object*);
                } else {
                    instance_offset += (1 << field_size);
                }


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

    return {};
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

    auto clz = (Class*)jvm::malloc(sizeof(Class));

    if (not clz) {
        puts("failed to alloc constant class memory");
        while (true)
            ; // TODO: raise error
    }

    new (clz) Class();

    clz->classfile_data_ = str;

    str += sizeof(ClassFile::HeaderSection1);


    clz->constants_ = new ConstantPoolCompactImpl();
    str = clz->constants_->parse(*h1);


    auto h2 = reinterpret_cast<const ClassFile::HeaderSection2*>(str);
    str += sizeof(ClassFile::HeaderSection2);

    if (h2->interfaces_count_.get()) {
        for (int i = 0; i < h2->interfaces_count_.get(); ++i) {
            // Each interface is merely an index into the constant pool.
            str += sizeof(u16);
        }
    }
    jvm::register_class(classname, clz);


    clz->super_ = jvm::load_class(clz, h2->super_class_.get());


    str = parse_classfile_fields(str, clz, *h1);

    auto h4 = reinterpret_cast<const ClassFile::HeaderSection4*>(str);
    str += sizeof(ClassFile::HeaderSection4);


    if (h4->methods_count_.get()) {

        clz->methods_ = (const ClassFile::MethodInfo**)jvm::malloc(
            sizeof(ClassFile::MethodInfo*) * h4->methods_count_.get());

        if (clz->methods_ == nullptr) {
            puts("failed to alloc method table");
            while (true)
                ; // TODO: raise error...
        }

        clz->method_count_ = h4->methods_count_.get();

        for (int i = 0; i < h4->methods_count_.get(); ++i) {

            auto method = (ClassFile::MethodInfo*)str;
            str += sizeof(ClassFile::MethodInfo*);

            clz->methods_[i] = method;

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

            if (attr_name == Slice::from_c_str("SourceFile")) {
                // TODO...
            } else {
                // TOOD...
            }
        }
    }

    return clz;
}



} // namespace java
