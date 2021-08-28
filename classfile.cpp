#include "classfile.hpp"
#include "class.hpp"
#include "vm.hpp"
#include <iostream>



const char* get_file_contents(const char* file);



namespace java {



const char* parse_classfile_fields(const char* str, Class* clz, int constant_count)
{
    // Here, we're loading the class file, and we want to determine all of the
    // correct byte offsets of all of the class instance's fields.

    auto h3 = reinterpret_cast<const ClassFile::HeaderSection3*>(str);
    str += sizeof(ClassFile::HeaderSection3);

    if (auto nfields = h3->fields_count_.get()) {

        auto fields = (SubstitutionField*)jvm::malloc(sizeof(SubstitutionField) * nfields);

        u16 offset = 0;

        clz->constants_->reserve_fields(nfields);

        for (int i = 0; i < nfields; ++i) {
            auto field = (const ClassFile::FieldInfo*)str;
            str += sizeof(ClassFile::FieldInfo);

            printf("field %d has offset %d\n", i, offset);

            fields[i].offset_ = offset;

            // printf("field desc %d\n", );

            SubstitutionField::Size field_size = SubstitutionField::b4;


            puts("load field type");
            auto field_type =
                clz->constants_->load_string(field->descriptor_index_.get());

            puts("load field name");
            auto field_name =
                clz->constants_->load_string(field->name_index_.get());


            if (field_type == Slice::from_c_str("I")) {
                field_size = SubstitutionField::b4;
            } else if (field_type == Slice::from_c_str("F")) {
                field_size = SubstitutionField::b4;
            } else if (field_type == Slice::from_c_str("S")) {
                field_size = SubstitutionField::b2;
            } else if (field_type == Slice::from_c_str("C")) {
                field_size = SubstitutionField::b1;
            } else {
                std::cout << std::string(field_type.ptr_, field_type.length_) << std::endl;
                puts("TODO: field sizes...");
                while (true) ;
            }

            fields[i].size_ = field_size;

            puts("try bind constant");
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

        auto clz = (Class*)jvm::malloc(sizeof(Class));

        if (not clz) {
            puts("failed to alloc constant class memory");
            while (true) ; // TODO: raise error
        }

        new (clz)Class();

        str += sizeof(ClassFile::HeaderSection1);


        clz->constants_ = new ConstantPoolCompactImpl();
        str = clz->constants_->parse(*h1);


        auto h2 = reinterpret_cast<const ClassFile::HeaderSection2*>(str);
        str += sizeof(ClassFile::HeaderSection2);


        clz->super_ = jvm::load_class(clz, h2->super_class_.get());


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
                jvm::malloc(sizeof(ClassFile::MethodInfo*)
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

        jvm::register_class(classname, clz);

        return clz;
    }

    return nullptr;
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
