# TODO: create a makefile. I threw this project together over the course of a
# few days, I haven't made a makefile/CMakeLists yet.

g++ -pedantic -Wall -O2 -std=c++14 -DPROJECT_ROOT=\"$(pwd)/\" -g src/vm.cpp src/class.cpp src/classtable.cpp src/jni.cpp src/classfile.cpp src/eb-java.cpp src/jar.cpp src/constantPool.cpp src/array.cpp src/crc32.cpp src/methodTable.cpp src/memory.cpp src/gc.cpp -o eb-java
