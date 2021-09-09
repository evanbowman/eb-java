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



struct Location {
    u8 type_;
    ObjectId class_id_;
    ObjectId method_id_;
    network_u64 index_;
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
        Location location_;
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


struct LineTableRequest {
    CommandPacket command_;
    ObjectId reference_type_id_; // the class
    ObjectId method_id_;         // the method pointer
};


struct LineTableReply {
    ReplyPacket reply_;
    network_s64 start_;
    network_s64 end_;
    network_s32 lines_;

    struct Row {
        network_s64 code_index_;
        network_s32 line_number_;
    };
};


struct CapabilitiesReply {
    ReplyPacket reply_;

    // NOTE: set these fields to false, until I get around to implementing these optional features.
    bool can_watch_field_modification = false;
    bool can_watch_field_access = false;
    bool can_get_bytecodes = true;
    bool can_get_synthetic_attribute = false;
    bool can_get_owned_monitor_info = false;
    bool can_get_current_contended_monitor = false;
    bool can_get_monitor_info = false;
    bool can_redefine_classes = false;
    bool can_add_method = false;
    bool can_unrestrictedly_redefine_classes = false;
    bool can_pop_frames = true;
    bool can_use_instance_filters = false;
    bool can_get_source_debug_extension = false;
    bool can_request_vm_death_event = false;
    bool can_set_default_stratum = false;
    bool can_get_instance_info = false;
    bool can_request_monitor_events = false;
    bool can_get_monitor_frameinfo = false;
    bool can_use_source_name_filters = false;
    bool can_get_constant_pool = true;
    bool can_force_early_return = false;
    bool reserved22 = false;
    bool reserved23 = false;
    bool reserved24 = false;
    bool reserved25 = false;
    bool reserved26 = false;
    bool reserved27 = false;
    bool reserved28 = false;
    bool reserved29 = false;
    bool reserved30 = false;
    bool reserved31 = false;
    bool reserved32 = false;
};


void listen();



}
}
}
