#include "vm.hpp"
#include "memory.hpp"
#include "jdwp.hpp"



extern "C" {
    void test_print(const char*);
}



namespace java {



void unhandled_error(const char* description)
{
    test_print(description);
    while (1)
        ;
}



void uncaught_exception(Slice cname,
                        Slice msg)
{
    while (true) ;
}



}



extern char __rom_end__;



int main()
{
    test_print("hello!\n");

    if (java::jvm::start_from_jar(&__rom_end__,
                                  java::Slice::from_c_str("game/startup/Boot"))) {
        test_print("error");
    }

    test_print("done!");
}
