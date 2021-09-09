#include "defines.hpp"
#include "jdwp.hpp"
#include <iostream>
#include "object.hpp"
#include "classtable.hpp"
#include "vm.hpp"



#if JVM_ENABLE_DEBUGGING
#include <SFML/Network.hpp>
#endif



// Java debug wire protocol. We don't implement nearly all of the functionality,
// and we're using a fairly old protocol version, because some of the older
// commands are simpler to implement.

// You may find that my code in this file is a complete mess. Not much
// documentation about interfacing with jdb out there, so I kind of had to
// figure things out by trial and error. That said, I actually think jdwp is a
// really pleasant and well-documented protocol, the only issue is that I
// sometimes have no idea about what exactly jdb wants me to send it, the
// debugger will sometimes freeze if you don't send it exactly what it expects.



namespace java {
namespace jvm {
namespace jdwp {



#if JVM_ENABLE_DEBUGGING



static sf::TcpSocket* g_client;


void send(const void* msg, size_t bytes)
{
    g_client->send(msg, bytes);
}



bool dot_to_slash_cpath_delimiter(Slice src, char* buffer, size_t buffer_size)
{
    if (src.length_ > buffer_size) {
        return false;
    }
    for (u32 i = 0; i < src.length_; ++i) {
        if (src.ptr_[i] == '.') {
            buffer[i] = '/';
        } else {
            buffer[i] = src.ptr_[i];
        }
    }
    return true;
}



static int outgoing_id = 2;



struct ClassPrepareEventRequest {
    network_u32 request_id_;
    Class* class_;
};



void handle_request(CommandPacket* msg)
{
    if (msg->header_.flags_ & 0x80) {
        auto rep = (ReplyPacket*)msg;
        // JDB sends us only odd ids. If we get an even id, it's simply a reply
        // from the debugger in response to one of our own commands.
        std::cout << rep->header_.length_.get()
                  << ", "
                  << rep->header_.id_.get()
                  << ", "
                  << (int)rep->header_.flags_
                  << ", "
                  << rep->header_.error_code_.get()
                  << std::endl;

        return;
    }

    std::cout << msg->header_.length_.get()
              << ", "
              << msg->header_.id_.get()
              << ", "
              << (int)msg->header_.flags_
              << ", "
              << (int)msg->header_.command_set_
              << ", "
              << (int)msg->header_.command_
              << ", "
              << std::endl;

    auto set_header = [&](ReplyPacket* repl, size_t size) {
        repl->header_.length_.set(size);
        repl->header_.id_.set(msg->header_.id_.get());
        repl->header_.flags_ = 0x80;
        repl->header_.error_code_.set(0);
    };

    switch (msg->header_.command_set_) {
    case 1: {
        switch (msg->header_.command_) {
        case 17: { // request capabilities
            CapabilitiesReply repl;
            set_header((ReplyPacket*)&repl, sizeof(repl));
            send(&repl, sizeof repl);
            break;
        }

        case 7: {
            IdSizesReply repl;
            set_header((ReplyPacket*)&repl, sizeof(repl));

            repl.field_id_size_.set(8);
            repl.method_id_size_.set(8);
            repl.object_id_size_.set(8);
            repl.reference_type_id_size_.set(8);
            repl.frame_id_size_.set(8);

            send(&repl, sizeof repl);
            break;
        }

        case 8: { // Suspend
            ReplyPacket repl;
            set_header((ReplyPacket*)&repl, sizeof(repl));
            send(&repl, sizeof repl);
            break;
        }

        case 1: {
            VersionReply repl;
            set_header((ReplyPacket*)&repl, sizeof(repl));

            repl.description_str_.length_.set(sizeof repl.description_);
            repl.vm_version_str_.length_.set(sizeof repl.vm_version_);
            repl.vm_name_str_.length_.set(sizeof repl.vm_name_);

            // TODO: The older the jdwp protocol I use, the less I have to
            // implement. We're using 1.4.
            repl.jdwp_major_.set(1);
            repl.jdwp_minor_.set(4);

            send(&repl, sizeof repl);
            break;
        }

        case 3: {
            auto class_count = classtable::size();

            // Ok, so this code has fairly large overhead. Because we're a small
            // microcontroller with limited ram, we cannot fit the entire
            // respose in memory at once. So we perform an initial pass of the
            // entire classtable, to pre-calculate the amount of bytes that
            // we're going to send (we're supposed to send the JDB
            // variable-length strings representing all of the classes in the
            // system).

            AllClassesReply repl;

            size_t response_length = sizeof(repl)
                + (sizeof(AllClassesReply::ClassInfoPreamble) * class_count)
                + (sizeof(AllClassesReply::ClassInfoFooter) * class_count);
            // response_length now represents the size of the response, without
            // the classname strings. Scan the classtable and add in the name
            // string lengths.

            classtable::visit([](Slice name, Class* clz, void* arg) {
                auto len = (size_t*)arg;
                *len += name.length_;
            }, &response_length);

            set_header((ReplyPacket*)&repl, response_length);

            repl.classes_count_.set(class_count);

            send(&repl, sizeof repl);

            classtable::visit([](Slice name, Class* clz, void*) {
                AllClassesReply::ClassInfoPreamble preamble;
                preamble.ref_type_tag_ = 1; // FIXME: Identify whether a class is an
                                            // interface.
                preamble.reference_type_id_.set((intptr_t)clz);
                preamble.signature_str_.length_.set(name.length_ + 2); // +2 for L and ;
                send(&preamble, sizeof preamble);

                send(name.ptr_, name.length_);

                AllClassesReply::ClassInfoFooter footer;
                footer.status_.set(4); // class initialized;

                send(&footer, sizeof footer);

            }, nullptr);

            break;
        }

        case 13: {
            ClasspathsReply repl;
            set_header((ReplyPacket*)&repl, sizeof repl);
            repl.base_dir_.length_.set(0);
            repl.classpaths_.set(0);
            repl.bootclasspaths_.set(0);

            send(&repl, sizeof repl);
            break;
        }
        }
        break;
    }

    case 2: {
        switch (msg->header_.command_) {
        case 10: {
            // TODO: actually respond with interaces...
            InterfacesReply repl;
            set_header((ReplyPacket*)&repl, sizeof repl);
            repl.count_.set(0);
            send(&repl, sizeof repl);
            break;
        }

        case 5: {
            auto clz = (Class*)((MethodsRequest*)msg)->reference_type_id_.get();

            MethodsReply repl;

            using Info = std::pair<size_t, int>;
            Info info;
            info.first = sizeof(repl);
            info.second = 0;

            // First: calculate the size of the data that we're going to send
            // back.
            clz->visit_methods([](Class* clz,
                                  const ClassFile::MethodInfo* mtd,
                                  void* arg) {

                auto info = (Info*)arg;

                const auto method_name_str =
                    clz->constants_->load_string(mtd->name_index_.get());

                const auto method_type_str =
                    clz->constants_->load_string(mtd->descriptor_index_.get());

                info->first
                    += method_name_str.length_
                    + method_type_str.length_
                    + sizeof(MethodsReply::MethodInfo1)
                    + sizeof(MethodsReply::MethodInfo2)
                    + sizeof(MethodsReply::MethodInfo3);

                info->second++;

            }, &info);

            set_header((ReplyPacket*)&repl, info.first);
            repl.method_count_.set(info.second);

            send(&repl, sizeof repl);


            // Actually send the data.
            clz->visit_methods([](Class* clz,
                                  const ClassFile::MethodInfo* mtd,
                                  void*) {

                const auto method_name_str =
                    clz->constants_->load_string(mtd->name_index_.get());

                const auto method_type_str =
                    clz->constants_->load_string(mtd->descriptor_index_.get());

                MethodsReply::MethodInfo1 m1;
                m1.reference_type_id_.set((intptr_t)mtd);
                m1.name_.length_.set(method_name_str.length_);
                send(&m1, sizeof m1);

                send(method_name_str.ptr_, method_name_str.length_);

                MethodsReply::MethodInfo2 m2;
                m2.signature_.length_.set(method_type_str.length_);
                send(&m2, sizeof m2);

                send(method_type_str.ptr_, method_type_str.length_);

                MethodsReply::MethodInfo3 m3;
                m3.mod_bits_.set(mtd->access_flags_.get());
                send(&m3, sizeof m3);

            }, nullptr);



            break;
        }
        }
        break;
    }

    case 3: {
        switch (msg->header_.command_) {
        case 1: {
            auto clz = (Class*)((SuperclassRequest*)msg)->reference_type_id_.get();
            SuperclassReply repl;
            set_header((ReplyPacket*)&repl, sizeof repl);
            repl.reference_type_id_.set((intptr_t)clz->super_);
            send(&repl, sizeof repl);
            break;
        }
        }
        break;
    }

    case 6: {
        switch (msg->header_.command_) {
        case 1: { // LineTable command
            auto req = (LineTableRequest*)msg;
            auto clz = (Class*)req->reference_type_id_.get();

            auto mtd = (const ClassFile::MethodInfo*)req->method_id_.get();

            if (auto linum_table = clz->get_line_number_table(mtd)) {

                s32 min = 999999999;
                s32 max = 0;

                for (int i = 0; i < linum_table->table_length_.get(); ++i) {
                    auto& row = linum_table->data()[i];
                    auto line = row.line_number_.get();
                    if (line < min) {
                        min = line;
                    }
                    if (line > max) {
                        max = line;
                    }
                }

                LineTableReply repl;
                repl.start_.set(min);
                repl.end_.set(max);
                repl.lines_.set(linum_table->table_length_.get());
                set_header((ReplyPacket*)&repl,
                           sizeof(repl) +
                           sizeof(LineTableReply::Row) *
                           linum_table->table_length_.get());
                send(&repl, sizeof repl);

                for (int i = 0; i < linum_table->table_length_.get(); ++i) {
                    auto row = linum_table->data()[i];

                    LineTableReply::Row out;
                    out.code_index_.set(row.start_pc_.get());
                    out.line_number_.set(row.line_number_.get());
                    send(&out, sizeof out);

                    std::cout << out.code_index_.get()
                              << ", "
                              << out.line_number_.get()
                              << std::endl;
                }

            } else {
                LineTableReply repl;
                set_header((ReplyPacket*)&repl, sizeof repl);
                repl.start_.set(-1);
                repl.end_.set(-1);
                repl.lines_.set(0);
                send(&repl, sizeof repl);
            }

            break;
        }
        }
        break;
    }

    case 15: {
        switch (msg->header_.command_) {
        case 2: { // clear
            // The debugger is asking us to clear an event request from the
            // system. Eventually, we'll need to actually keep track of
            // requests, in case we need to clear a breakpoint or something like
            // that. For now, just do nothing and send an ack.
            ReplyPacket repl;
            set_header(&repl, sizeof repl);
            send(&repl, sizeof repl);
            break;
        }

        case 1: { // set
            Slice class_pattern;
            Location loc;

            auto req = (EventRequest*)msg;
            std::cout << (int)req->event_kind_
                      << ", "
                      << req->modifier_count_.get()
                      << std::endl;

            auto data = ((u8*)msg) + sizeof(EventRequest);

            for (u32 i = 0; i < req->modifier_count_.get(); ++i) {
                auto kind = ((EventRequest::Modifier*)data)->mod_kind_;
                switch (kind) {
                case 1: {
                    data += sizeof(EventRequest::CaseCountModifier);
                    break;
                }

                case 5: {
                    auto mod = (EventRequest::ClassMatchModifier*)data;
                    data += sizeof(EventRequest::ClassMatchModifier);
                    data += mod->pattern_.length_.get();
                    class_pattern = Slice(mod->pattern_data(),
                                          mod->pattern_.length_.get());

                    std::cout << std::string(mod->pattern_data(),
                                             mod->pattern_.length_.get())
                              << std::endl;
                    break;
                }

                case 7: {
                    auto mod = (EventRequest::LocationOnlyModifier*)data;
                    data += sizeof(EventRequest::LocationOnlyModifier);
                    //
                    std::cout << "got location modifier " << mod->location_.index_.get()
                              << std::endl;
                    break;
                }

                case 8: {
                    auto mod = (EventRequest::ExceptionOnlyModifier*)data;
                    data += sizeof(EventRequest::ExceptionOnlyModifier);
                    if (auto clz = (Class*)mod->exception_or_null_.get()) {
                        printf("exception only mod for %p\n", (void*)clz);
                        std::cout << mod->caught_ << ", "
                                  << mod->uncaught_
                                  << std::endl;
                        // TODO: what to do with exceptiononly modifier for
                        // event request?
                    }
                    break;
                }

                default:
                    std::cout << "unsupported modifier type "
                              << (int)kind
                              << std::endl;
                    while (1);
                }
            }

            EventRequestReply repl;
            set_header((ReplyPacket*)&repl, sizeof repl);

            static int requestid = 1;
            repl.request_id_.set(requestid++);

            send(&repl, sizeof repl);

            std::cout << "req " << (int)req->event_kind_ << std::endl;


            switch (req->event_kind_) {
            case 8: {
                if (class_pattern.length_) {
                    char buffer[80];
                    dot_to_slash_cpath_delimiter(class_pattern, buffer, 80);
                    if (auto clz = load_class_by_name(Slice(buffer,
                                                            class_pattern.length_))) {
                        EventData data;
                        data.command_.header_.length_
                            .set(sizeof(data) +
                                 sizeof(EventData::ClassPrepareEvent) +
                                 sizeof(EventData::ClassPrepareEventFooter) +
                                 class_pattern.length_);

                        std::cout << "len " <<
                            data.command_.header_.length_.get()
                                  << ", "
                                  << sizeof(data)
                                  << std::endl;

                        data.command_.header_.id_.set(outgoing_id);
                        outgoing_id += 2; // Is this method for generating
                                          // outgoing ids even correct?
                        data.command_.header_.flags_ = 0;
                        data.command_.header_.command_set_ = 64; // Event
                        data.command_.header_.command_ = 100; // Composite
                        data.suspend_policy_ = 0;
                        data.event_count_.set(1);// 1);
                        send(&data, sizeof data);

                        EventData::ClassPrepareEvent event;
                        event.event_.event_kind_ = 8; // prepare
                        event.request_id_.set(repl.request_id_.get());
                        event.thread_id_.set(1); // ?
                        event.ref_type_tag_ = ref_type_class;
                        event.ref_type_id_.set((intptr_t)clz);
                        event.signature_.length_.set(2 + class_pattern.length_);
                        send(&event, sizeof event);

                        send(buffer, class_pattern.length_);

                        EventData::ClassPrepareEventFooter footer;
                        footer.status_.set(class_status_prepared);
                        send(&footer, sizeof footer);
                    }
                }
                break;
            }

            case 9: // class unload
                break;

            case 6: // thread death
                break;
            }


            break;
        }
        }
        break;
    }
    }
}



void listen()
{
    sf::TcpListener listener;

    // bind the listener to a port
    if (listener.listen(59012) != sf::Socket::Done) {
        // error...
    }

    // accept a new connection
    sf::TcpSocket client;
    client.setBlocking(true);
    if (listener.accept(client) != sf::Socket::Done) {
        // error...
    }


    // Receive an answer from the server
    char buffer[1024];
    std::size_t received = 0;
    client.receive(buffer, sizeof(buffer), received);

    if (received < sizeof(Handshake)) {
        puts("receive error!");
    } else {
        client.send(buffer, sizeof(Handshake));
    }

    g_client = &client;

    while (true) {
        char buffer[1024];
        std::size_t received = 0;
        puts("waiting...");
        client.receive(buffer, sizeof(buffer), received);

        if (received) {
            handle_request((CommandPacket*)buffer);
        }
    }
}



#else


void listen()
{

}


#endif // JVM_ENABLE_DEBUGGING




}
}
}
