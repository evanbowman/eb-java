#pragma once

#include "classfile.hpp"
#include "slice.hpp"
#include <stdio.h>


// NOTE: Creating a constant pool in memory for every class takes up a lot of
// space. We have a number of implementations of a constant pool, some are
// backed by an allocation, and some run directly from the class file (much
// slower).



namespace java {



class ConstantPool {
public:
    virtual ~ConstantPool()
    {
    }


    virtual const char* parse(const ClassFile::HeaderSection1& src) = 0;


    virtual const ClassFile::ConstantHeader* load(u16 index) = 0;


    virtual void reserve_fields(int count) = 0;


    virtual void bind_field(u16 index, void* data) = 0;


    Slice load_string(u16 index)
    {
        auto constant = load(index);

        if (constant->tag_ == ClassFile::ConstantType::t_utf8) {
            return Slice {
                (const char*)constant + sizeof(ClassFile::ConstantUtf8),
                ((ClassFile::ConstantUtf8*)constant)->length_.get()
            };
        } else {
            while (true) ;
        }
    }
};



// The fastest constant pool implementation. All lookups are O(1), as the class
// stores an array of pointers into the classfile.
class ConstantPoolArrayImpl : public ConstantPool {
public:


    const char* parse(const ClassFile::HeaderSection1& src) override;


    const ClassFile::ConstantHeader* load(u16 index) override
    {
        return array_[index - 1];
    }


    void reserve_fields(int) override
    {

    }


    void bind_field(u16 index, void* field)
    {
        array_[index - 1] = (const ClassFile::ConstantHeader*)field;
    }


private:
    const ClassFile::ConstantHeader** array_ = nullptr;
};



// This class uses minimal extra memory.
class ConstantPoolCompactImpl : public ConstantPool {
public:


    // Requires O(n), where n is the number of constants in the constant pool.
    const ClassFile::ConstantHeader* load(u16 index) override
    {
        index -= 1;

        for (int i = 0; i < binding_count_; ++i) {
            if (bindings_[i].index_ == index and bindings_[i].field_) {
                return (const ClassFile::ConstantHeader*)bindings_[i].field_;
            }
        }

        auto str = (const char*)info_ + sizeof(ClassFile::HeaderSection1);

        int i = 0;
        while (i not_eq index) {
            str += ClassFile::constant_size((const ClassFile::ConstantHeader*)str);
            ++i;
        }

        return (ClassFile::ConstantHeader*)str;
    }


    const char* parse(const ClassFile::HeaderSection1& src) override
    {
        info_ = &src;

        const char* str =
            ((const char*)&src) + sizeof(ClassFile::HeaderSection1);

        for (int i = 0; i < src.constant_count_.get() - 1; ++i) {
            str += ClassFile::constant_size((const ClassFile::ConstantHeader*)str);
        }

        return str;
    }


    struct FieldBinding {
        u16 index_;
        void* field_;
    };


    void reserve_fields(int count) override;


    void bind_field(u16 index, void* field) override
    {
        if (field == nullptr) {
            // Hmm... should never really happen...
            return;
        }

        for (int i = 0; ; ++i) {
            if (bindings_[i].field_ == nullptr) {
                bindings_[i].index_ = index - 1;
                bindings_[i].field_ = field;
                return;
            }
        }
    }


private:
    FieldBinding* bindings_ = nullptr;
    const ClassFile::HeaderSection1* info_;
    u16 binding_count_ = 0;
};



// A balanced constant pool class. More compact than a large array
// implementation, but caches some recent constant lookups in a local buffer.
class ConstantPoolCompactCachingImpl : public ConstantPoolCompactImpl {
private:

    struct {
        u16 index_ = 0;
        const ClassFile::ConstantHeader* constant_ = nullptr;
    } cache_[4];

    u8 evict_ = 0;

public:

    const ClassFile::ConstantHeader* load(u16 index) override
    {
        for (auto& elem : cache_) {
            if (elem.index_ == index and elem.constant_) {
                return elem.constant_;
            }
        }

        auto found = ConstantPoolCompactImpl::load(index);

        cache_[evict_++ % 4] = {index, found};

        return found;
    }
};



}
