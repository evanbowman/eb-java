#pragma once

#include "slice.hpp"


// NOTE: This jar library does not store anything in memory. It accepts a
// pointer to a jar file, and returns a pointer to a class within the input
// buffer.

// NOTE: we want our jvm to run on small embedded systems, therefore, jars must
// be uncompressed.


namespace java {
namespace jar {



Slice load_file_data(const char* jar_file_bytes, Slice path);



Slice load_classfile(const char* jar_file_byts, Slice classpath);



}
}
