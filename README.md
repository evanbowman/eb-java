# mbed-jvm

A small Java virtual machine implementation, runs with limited memory. Intended for microcontrollers. Each class occupies twenty-eight bytes in a 32-bit system. Each member field within a class allocates a two-byte descriptor. All instances of objects are packed, so each member field is layed out in memory with no wasted space in between. I have not finished the GC yet, but I will be implementing a compacting collector. This project is in its early stages, I've only been working on it for a few days.

Some of the source code in the project may appear overcomplicated, delicate... a house of cards waiting to collapse. I wanted to develop an implementation that can run directly from the classfile data, to minimize extra memory usage. Doing so complicated the implementation in several ways.
