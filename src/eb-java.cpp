#include "vm.hpp"
#include "memory.hpp"


#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>



namespace java {



void unhandled_error(const char* description)
{
    puts(description);
    while (1)
        ;
}



void uncaught_exception(Slice cname,
                        Slice msg)
{
    std::cout
        << "program terminated after throwing exception of class "
        << std::string(cname.ptr_, cname.length_) << ", message: "
        << std::string(msg.ptr_, msg.length_)
        << std::endl;

    while (true) ;
}



}



int main(int argc, char** argv)
{
    if (argc != 3) {
        puts("usage: eb-java <jar|classfile> <classpath>");
        return 1;
    }

    std::string fname(argv[1]);

    std::ifstream t(fname);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());

    const auto classpath = java::Slice::from_c_str(argv[2]);

    if (fname.substr(fname.find_last_of(".") + 1) == "jar") {
        auto status = java::jvm::start_from_jar(str.c_str(), classpath);
        java::jvm::heap::print_stats([](const char* str) { printf("%s", str); });
        return status;
    } else {
        auto status = java::jvm::start_from_classfile(str.c_str(), classpath);
        java::jvm::heap::print_stats([](const char* str) { printf("%s", str); });
        return status;
    }

    return 0;
}
