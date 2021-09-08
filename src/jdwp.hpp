#pragma once

#include "endian.hpp"
#include "object.hpp"



// Java Debug Wire Protocol



namespace java {
namespace jvm {
namespace jdwp {



using ObjectId = network_u64;



struct Handshake {
    char message_[14] = {
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




struct IdSizesReply {
    ReplyPacket reply_;
    network_u32 field_id_size_;
    network_u32 method_id_size_;
    network_u32 object_id_size_;
    network_u32 reference_type_id_size_;
    network_u32 frame_id_size_;
};



struct String {
    network_u32 length_;

    const char* data()
    {
        return ((const char*) this) + length_.get();
    }
};



enum {
    ref_type_class = 1,
    ref_type_interface,
    ref_type_array
};


enum {
    class_status_verified = 1,
    class_status_prepared = 2,
    class_status_initialized = 4,
    class_status_error = 8
};



struct EventData {
    CommandPacket command_;
    u8 suspend_policy_;
    network_u32 event_count_;

    // Event events_[event_count_];

    struct Event {
        u8 event_kind_;
    };

    struct ClassPrepareEvent {
        Event event_;
        network_u32 request_id_;
        ObjectId thread_id_;
        u8 ref_type_tag_;
        ObjectId ref_type_id_;
        String signature_;
        char signature_begin_ = 'L';
    };

    struct ClassPrepareEventFooter
    {
        char signature_end_ = ';';
        network_u32 status_;
    };
};



struct EventRequest {
    CommandPacket command_;
    u8 event_kind_;
    u8 suspend_policy_;
    network_u32 modifier_count_;

    // Modifier modifiers_[modifier_count_];

    struct Modifier {
        u8 mod_kind_;
        // u8 data_[...];
    };

    struct CaseCountModifier {
        Modifier mod_;
        network_u32 count_;
    };

    struct ConditionalModifier {
        Modifier mod_;
        network_u32 expr_id_;
    };

    struct ThreadOnlyModifier {
        Modifier mod_;
        ObjectId thread_;
    };

    struct ClassOnlyModifier {
        Modifier mod_;
        ObjectId class_id_;
    };

    struct ClassMatchModifier {
        Modifier mod_;
        String pattern_;

        const char* pattern_data()
        {
            return ((char*)this) + sizeof *this;
        }
    };

    struct ClassExcludeModifier {
        Modifier mod_;
        String pattern_;
    };

    struct LocationOnlyModifier {
        Modifier mod_;
        ObjectId location_;
    };

    struct ExceptionOnlyModifier {
        Modifier mod_;
        ObjectId exception_or_null_;
        bool caught_;
        bool uncaught_;
    };

};



struct EventRequestReply {
    ReplyPacket reply_;
    network_u32 request_id_;
};



struct InterfacesRequest {
    CommandPacket command_;
    ObjectId reference_type_id_;
};



struct InterfacesReply {
    ReplyPacket reply_;
    network_u32 count_;
};



struct VersionReply {
    ReplyPacket reply_;

    String description_str_;
    char description_[11] = {
        'E', 'B', 'J', 'V', 'M', '-', 'D', 'E', 'B', 'U', 'G'
    };

    network_u32 jdwp_major_;
    network_u32 jdwp_minor_;

    String vm_version_str_;
    char vm_version_[9] = {
        '1', '.', '8', '.', '0', '_', '2', '1', '1',
    };

    String vm_name_str_;
    char vm_name_[5] = {
        'E', 'B', 'J', 'V', 'M'
    };
};



struct AllClassesReply {
    ReplyPacket reply_;
    network_u32 classes_count_;

    struct ClassInfoPreamble {
        u8 ref_type_tag_;
        ObjectId reference_type_id_;

        String signature_str_;
        char signature_begin_ = 'L';
        // u8 signature_[...];
    };

    struct ClassInfoFooter {
        char signature_end_ = ';';
        network_s32 status_;
    };
};



struct ClasspathsReply {
    ReplyPacket reply_;
    // All of these fields intentionally left empty.
    String base_dir_;
    network_u32 classpaths_;
    network_u32 bootclasspaths_;
};



struct InfoRequest {
    CommandPacket command_;
    ObjectId reference_type_id_;
};



using SuperclassRequest = InfoRequest;
using MethodsRequest = InfoRequest;


struct SuperclassReply {
    ReplyPacket reply_;
    ObjectId reference_type_id_; // superclass
};


struct MethodsReply {
    ReplyPacket reply_;
    network_u32 method_count_;

    // ...

    struct MethodInfo1 {
        ObjectId reference_type_id_; // method pointer
        String name_;
    };

    struct MethodInfo2 {
        String signature_;
    };

    struct MethodInfo3 {
        network_u32 mod_bits_;
    };
};



void listen();



}
}
}
