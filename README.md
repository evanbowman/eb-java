Regression: [![Build Status](https://app.travis-ci.com/evanbowman/eb-java.svg?branch=master)](https://app.travis-ci.com/evanbowman/eb-java)


# eb-java

A small Java virtual machine implementation, runs with limited memory. Intended for microcontrollers.
By default, the virtual machine uses 256kb of RAM for its heap, and a bit more memory for the operand stack.

I have not finished this project yet, but eb-java does currently implement almost all of the Java instruction set (I'm still working on invokedynamic :)), and I've written a number of unit tests.

The project, in its current form, compiles a command line application called eb-java:
```
usage: eb-java <jar|classfile> <classpath>
```
eb-java supports jar files, but you need to strip compression from the jars before running them. The vm does not support compressed jars, as I'm intending to use this code on a microcontroller (gba), where compressed jars would limit the size of an executable due to limited ram. See the unit test directory, where I build a jar without compression.


Remaining work:
* Implement more of the standard JRE classes
* Finish implementing invokedynamic
* More unit tests
* Realistically, there are problably a couple of bugs, which will hopefully be uncovered by extensive unit testing


Limitations:
* I've bundled the VM with a subset of classes from java.lang that I thought would be useful. The project does not currently include even remotely all of the classes in java.base, partly because many OpenJDK classes are dependent on other JDK classes, so it's hard to write even simple java code without importing dozens of JRE classes. I've been slowly building a carefully curated subset of the JDK-8 JRE that I think would be useful for software development. Alternatively, I could just drop the whole OpenJDK-8 JRE into the project, and in fact, it would probably run ok-ish. 
* Even when I finish implementing the InvokeDynamic instruction, it will be a while before I get around to implementing LambdaMetaFactory, StringConcatFactory, etc.
* The VM implementation does not support single objects larger than 2047 bytes (no limitation on arrays, though, other than the heap size). You would need to put quite a lot of fields in a class to exceed the limit, though... the largest datatype, a long integer, occupies eight bytes, so 255 long integers in a single class (or 511 int variables).
* The default heap occupies 256kb. The system can be configured with a larger heap, but currently, you should not configure the heap to anything larger than 256mb (the theoretical upper limit).
* No support for jars with zip compression. None planned.

## Internals

### Memory Layout

The vm implementation allocates Objects, Classes, and metadata from a single contiguous heap. The system allocates objects from the beginning of the heap, and class metadata from the end of the heap. When the objects and the metadata collide, the virtual machine runs a compacting garbage collector, to free up space within the object region of the heap, leaving room for more instances or metadata. The vm never deallocates metadata. When the heap compactor fails to free up enough bytes for another allocation, the VM halts with an out of memory error.
```
Heap Chart   (*) used   (.) unused
*************...................................................................
*************...................................................................
*************...................................................................
************....................................................................
************...................................................................*
objects 40232 bytes -->                                 <-- class info 832 bytes
heap used 41064, remaining 214936
```

Classes themselves occupy about twenty-four bytes (assuming 32 bit), and the classtable datastructure requires a few additional bytes per class. Each field defined in a class will allocate a two-byte descriptor, to speed up field lookup (otherwise we'd need to essentially perform dynamic linking whenever a class accesses a field). Static variables also allocate descriptors, which are a bit larger than instance field descriptors, as static fields are singletons, and I have not spent as much time optimizing storage for statics. The implementation packs all object instances, so the vm stores class fields in memory with no space in-between (which saves a lot of memory, but does make loading fields sligtly slower, as packing objects introduces some extra copies due to alignment).

A class may or may not also include a method cache. I plan to add a method call on the java side of things, allowing the user to declare which essential classes should allocate a method cache. Currently, only classes with native methods use a method cache, as I need somewhere to bind the jni methods. Classes without method caches perform the vm's slow-path method lookup, which scans the class chain and runs directly against the classfile (requiring no additional memory). The Java standard libraries include tons of infrequently-used classes, so I do not intend to allocate method caches by default.

## Disclaimer

I work on this every once in a while for fun. The VM implementation runs java programs fairly accurately, but a purely interpreted, limited memory JVM will never be very fast. Also, I haven't implemented very many standard JRE classes, so you cannot do much with this JVM right now. I originally designed the code to run on a gameboy advance, but later determined that it's not really fast enough for realtime applications, and probably never will be, without throwing memory at it, which you can't do in an embedded cpu. The java compiler's disinterest in emitting optimized bytecode does not help performance on small microchips either. Still, I enjoy working on this from time to time.
