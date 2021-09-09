#pragma once


#ifndef JVM_USE_CALLSTACK
#define JVM_USE_CALLSTACK 1
#endif


#ifndef JVM_HEAP_SIZE
#define JVM_HEAP_SIZE 256000
#endif


#ifndef CLASSTABLE_SIZE
#define CLASSTABLE_SIZE 128
#endif


#ifndef JVM_METHOD_CACHE_SIZE
#define JVM_METHOD_CACHE_SIZE 8
#endif


#ifndef JVM_OPERAND_STACK_SIZE
#define JVM_OPERAND_STACK_SIZE 512
#endif


#ifndef JVM_STACK_LOCALS_SIZE
#define JVM_STACK_LOCALS_SIZE 1024
#endif


#ifndef JVM_STACK_OVERFLOW_CHECK
#define JVM_STACK_OVERFLOW_CHECK 0
#endif


#ifndef JVM_ENABLE_DEBUGGING
#define JVM_ENABLE_DEBUGGING 0
#endif


#ifndef JVM_AVAILABLE_BREAKPOINTS
#define JVM_AVAILABLE_BREAKPOINTS 4
#endif


#if JVM_ENABLE_DEBUGGING
#if not JVM_USE_CALLSTACK
#error "Debugging requires a callstack"
#endif
#endif
