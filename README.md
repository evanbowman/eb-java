# eb-java

A small Java virtual machine implementation, runs with limited memory. Intended for microcontrollers. Each class occupies twenty-four bytes in a 32-bit system. Each member field within a class allocates a two-byte descriptor. All instances of objects are packed, so each member field is layed out in memory with no wasted space in between. I have not finished the GC yet, but I will be implementing a compacting collector. This project is in its early stages, I've only been working on it for a few days.

Some of the source code in the project may appear overcomplicated, delicate... a house of cards waiting to collapse. I wanted to develop an implementation that can run directly from the classfile data, to minimize extra memory usage. Doing so complicated the implementation in several ways.

By default, the virtual machine uses 256kb of RAM for its heap, and a bit more memory for the operand stack.

I have not finished this project yet, but eb-java does currently implement almost all of the Java instruction set (I'm still working on invokedynamic :)), and I've written a number of unit tests.
