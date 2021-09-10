#pragma once

#include "class.hpp"



namespace java {
namespace jvm {
namespace debugger {



struct Breakpoint {
    Class* clz_;
    const ClassFile::MethodInfo* method_;
    u32 bytecode_offset_;
    u32 req_;
    Breakpoint* next_;
};



void init();



void update(Class* current_clz,
            const ClassFile::MethodInfo* current_mtd,
            u32 current_pc);



} // namespace debugger
} // namespace jvm
} // namespace java
