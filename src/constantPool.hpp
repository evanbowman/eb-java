#pragma once

#include "classfile.hpp"
#include "slice.hpp"
#include "substitutionField.hpp"
#include <iostream>
#include <stdio.h>


// NOTE: Creating a constant pool in memory for every class takes up a lot of
// space. We have a number of implementations of a constant pool, some are
// backed by an allocation, and some run directly from the class file.



namespace java {



class ConstantPool {
public:
    virtual ~ConstantPool()
    {
    }


    struct FieldBinding {
        u16 index_;
        SubstitutionField field_;
    };


    virtual const char* parse(const ClassFile::HeaderSection1& src) = 0;


    virtual const ClassFile::ConstantHeader* load(u16 index) = 0;


    virtual void reserve_fields(int count) = 0;


    virtual void bind_field(u16 index, SubstitutionField field) = 0;


    virtual std::pair<const FieldBinding*, u16> bindings()
    {
        while (true)
            ;
    }


    Slice load_string(u16 index)
    {
        auto constant = load(index);

        if (constant->tag_ == ClassFile::ConstantType::t_utf8) {
            return Slice{(const char*)constant +
                             sizeof(ClassFile::ConstantUtf8),
                         ((ClassFile::ConstantUtf8*)constant)->length_.get()};
        } else {
            while (true)
                ;
        }
    }
};



// The fastest constant pool implementation. All lookups are O(1), as the class
// stores an array of pointers into the classfile.
//
// While this class speeds things up somewhat over the ConstantPoolCompactImpl,
// the main performance benefit shows up when classes have a really big constant
// pool, in which case we may not want to allocate a big table, as this jvm is
// intended for small microcontrollers. In other words, this class improves
// performance over the ConstantPoolCachingImpl, but not necessarily enough of
// an improvement to make this the default constant pool, considering the
// increased memory usage (note: I'm not speculating here, I have in fact
// measured the performance).
//
// FIXME: this ArrayImpl code maybe out of date, and should be retested
// before being used in this project.
class ConstantPoolArrayImpl : public ConstantPool {
public:
    const char* parse(const ClassFile::HeaderSection1& src) override;


    const ClassFile::ConstantHeader* load(u16 index) override
    {
        return array_[index - 1];
    }


    void reserve_fields(int field_count) override;


    void bind_field(u16 index, SubstitutionField field) override;


private:
    const ClassFile::ConstantHeader** array_ = nullptr;
    SubstitutionField* fields_ = nullptr;
};



// This class uses minimal extra memory.
class ConstantPoolCompactImpl : public ConstantPool {
public:
    // Runs in O(n), where n is the number of constants in the constant pool.
    const ClassFile::ConstantHeader* load(u16 index) override
    {
        index -= 1;

        for (int i = 0; i < binding_count_; ++i) {
            if (bindings_[i].index_ == index and bindings_[i].field_.valid_) {
                return (const ClassFile::ConstantHeader*)&bindings_[i].field_;
            }
        }

        auto str = (const char*)info_ + sizeof(ClassFile::HeaderSection1);

        int i = 0;

        while (i not_eq index) {
            auto c = (const ClassFile::ConstantHeader*)str;
            if (c->tag_ == ClassFile::t_double or
                c->tag_ == ClassFile::t_long) {
                ++i;
            }
            str += ClassFile::constant_size(c);
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
            auto c = (const ClassFile::ConstantHeader*)str;

            str += ClassFile::constant_size(c);

            if (c->tag_ == ClassFile::t_double or
                c->tag_ == ClassFile::t_long) {
                ++i;
            }
        }

        return str;
    }


    void reserve_fields(int count) override;


    void bind_field(u16 index, SubstitutionField field) override
    {
        for (int i = 0;; ++i) {
            if (not bindings_[i].field_.valid_) {
                bindings_[i].index_ = index - 1;
                bindings_[i].field_ = field;
                return;
            }
        }
    }


    std::pair<const FieldBinding*, u16> bindings() override
    {
        return {bindings_, binding_count_};
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
    static const int cache_entries = 4;

    struct CacheEntry {
        u16 index_ = 0;
        const ClassFile::ConstantHeader* constant_ = nullptr;
    } cache_[cache_entries];

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

        CacheEntry cached;
        cached.index_ = index;
        cached.constant_ = found;
        cache_[evict_++ % cache_entries] = cached;

        return found;
    }
};



} // namespace java
