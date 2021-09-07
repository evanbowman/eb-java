#pragma once

#include "slice.hpp"



namespace java {

// Library user must define these two functions.
[[noreturn]] void unhandled_error(const char* description);
[[noreturn]] void uncaught_exception(Slice exception_class_name,
                                     Slice exception_message);

}
