#include "classtable.hpp"
#include "crc32.hpp"
#include "memory.hpp"
#include <iostream>



#ifndef CLASSTABLE_SIZE
#define CLASSTABLE_SIZE 128
#endif



namespace java {
namespace jvm {
namespace classtable {



struct ClassTableEntry {
    Slice name_;
    Class* class_;
    ClassTableEntry* next_;
};



static ClassTableEntry* class_table[CLASSTABLE_SIZE];



static inline u32 table_index(Slice name)
{
    return crc32(name.ptr_, name.length_) % CLASSTABLE_SIZE;
}



void insert(Slice name, Class* clz)
{
    const auto index = table_index(name);

    auto entry = classmemory::allocate<ClassTableEntry>();
    entry->name_ = name;
    entry->class_ = clz;
    entry->next_ = class_table[index];

    class_table[index] = entry;
}



Class* load(Slice name)
{
    auto search = class_table[table_index(name)];

    while (search) {
        if (search->name_ == name) {
            return search->class_;
        }

        search = search->next_;
    }
    return nullptr;
}



void visit(void (*visitor)(Class*))
{
    for (auto row : class_table) {
        while (row) {
            visitor(row->class_);
            row = row->next_;
        }
    }
}



Slice name(Class* clz)
{
    for (auto row : class_table) {
        while (row) {
            if (row->class_ == clz) {
                return row->name_;
            }
            row = row->next_;
        }
    }

    return Slice{};
}



} // namespace classtable
} // namespace jvm
} // namespace java
