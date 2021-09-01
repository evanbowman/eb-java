# TODO: create a makefile. I threw this project together over the course of a
# few days, I haven't made a makefile/CMakeLists yet.

g++ -pedantic -Wall -O2 -std=c++14 -g vm.cpp class.cpp jni.cpp classfile.cpp jar.cpp constantPool.cpp array.cpp memory.cpp -o java
