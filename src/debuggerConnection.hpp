#pragma once

#include "int.h"



namespace java {
namespace jvm {
namespace debugger {
namespace connection {



// NOTE: I wrote this file so that I could easily create an implementation for
// embedded microcontrollers. If we weren't dealing with interrupt handlers and
// memory constraints, the api could perhaps be made a bit simpler.



// JDWP does not send the vm very large messages. In fact, 128 bytes is quite
// generous, assuming that we can keep up with the sender.
static const u32 buffer_size = 128;



using Buffer = u8*;



void init(Buffer in);



bool is_connected();



// Non-blocking. Accepts Buffer in, yields buffer out, returns number of bytes
// written to Buffer out. The caller should swap in/out each time it calls
// receive.
u32 receive(Buffer in, Buffer* out);



// Blocking.
void send(const void* data, u32 len);



} // namespace connection
} // namespace debugger
} // namespace jvm
} // namespace java
