Regression: [![Build Status](https://app.travis-ci.com/evanbowman/eb-java.svg?branch=master)](https://app.travis-ci.com/evanbowman/eb-java)


# eb-java

A small Java virtual machine implementation, runs with limited memory. Intended for microcontrollers. Each class occupies twenty-four bytes in a 32-bit system. Each member field within a class allocates a two-byte descriptor. All instances of objects are packed, so each member field is layed out in memory with no wasted space in between. Includes a compacting garbage collector. This project is in its early stages, I've only been working on it for a few days.

By default, the virtual machine uses 256kb of RAM for its heap, and a bit more memory for the operand stack.

I have not finished this project yet, but eb-java does currently implement almost all of the Java instruction set (I'm still working on invokedynamic :)), and I've written a number of unit tests.

The project, in its current form, compiles a command line application called java:
```
usage: java <jar|classfile> <classpath>
```
eb-java supports jar files, but you need to strip compression from the jars before running them. The vm does not support compressed jars, as I'm intending to use this code on a microcontroller (gba), where compressed jars would limit the size of an executable due to limited ram. See the unit test directory, where I build a jar without compression.


Remaining work:
* Implement more of the standard JRE classes
* Finish implementing invokedynamic
* Multidimensional Arrays
* More unit tests
* Realistically, there are problably a couple of bugs, which will hopefully be uncovered by extensive unit testing


Limitations:
* The VM implementation does not support single objects larger than 2047 bytes (no limitation on arrays, though, other than the heap size). You would need to put quite a lot of fields in a class to exceed the limit, though... the largest datatype, a long integer, occupies eight bytes, so 255 long integers in a single class (or 511 int variables).
* The default heap occupies 256kb. The system can be configured with a larger heap, but currently, you should not configure the heap to anything larger than 256mb (the theoretical upper limit).
* No support for jars with zip compression. None planned.
