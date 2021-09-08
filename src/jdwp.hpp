#pragma once

#include "endian.hpp"



// Java Debug Wire Protocol



namespace java {
namespace jvm {
namespace jdwp {



struct Handshake {
    char message[14] = {
        'J', 'D', 'W', 'P', '-', 'H', 'a', 'n', 'd', 's', 'h', 'a', 'k', 'e'
    };
};



struct CommandPacket {
    struct Header {
        network_u32 length_;
        network_u32 id_;
        u8 flags_;
        u8 command_set_;
        u8 command_;
    } header_;
    // u8 data_[...];
};



struct ReplyPacket {
    struct Header {
        network_u32 length_;
        network_u32 id_;
        u8 flags_;
        network_u16 error_code_;
    } header_;
    // u8 data_[...];
};



}
}
}
