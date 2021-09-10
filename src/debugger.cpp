#include "debugger.hpp"
#include "debuggerConnection.hpp"
#include "buffer.hpp"
#include "defines.hpp"
#include "jdwp.hpp"



#if JVM_ENABLE_DEBUGGING


namespace java {
namespace jvm {
namespace debugger {



enum class State {
    await_connection,
    handshake,
    ready,
    halted,
} state = State::await_connection;



static u8 rd_buffer_1[connection::buffer_size];
static u8 rd_buffer_2[connection::buffer_size];



static Breakpoint breakpoint_memory[JVM_AVAILABLE_BREAKPOINTS];
static Breakpoint* free_breakpoints;
static Breakpoint* active_breakpoints;



static connection::Buffer back_buffer = rd_buffer_2;



void init()
{
    for (auto& br : breakpoint_memory) {
        br.next_ = free_breakpoints;
        free_breakpoints = &br;
    }

    connection::init(rd_buffer_1);
}



Buffer<u8, connection::buffer_size> rd_bytes;



void receive()
{
    auto received = connection::receive(back_buffer, &back_buffer);

    for (u32 i = 0; i < received; ++i) {
        rd_bytes.push_back(back_buffer[i]);
    }
}



void consume(u32 bytes)
{
    if (bytes > rd_bytes.size()) {
        unhandled_error("debugger rd_buffer logic error!");
    }

    for (u32 i = 0; i < rd_bytes.size() - bytes; ++i) {
        rd_bytes[i] = rd_bytes[bytes + i];
    }

    for (u32 i = 0; i < bytes; ++i) {
        rd_bytes.pop_back();
    }
}



bool hit_breakpoint(const Breakpoint& br,
                    Class* current_clz,
                    const ClassFile::MethodInfo* current_mtd,
                    u32 current_pc)
{
    return br.clz_ == current_clz and br.method_ == current_mtd and
           br.bytecode_offset_ == current_pc;
}



void process_command()
{
    receive();

    static_assert(sizeof(jdwp::CommandPacket) == sizeof(jdwp::ReplyPacket),
                  "size check with rd_bytes and CommandPacket "
                  "(below) will be erroneous");

    if (rd_bytes.size() >= sizeof(jdwp::CommandPacket)) {
        auto packet_len =
            ((jdwp::CommandPacket*)rd_bytes.begin())->header_.length_.get();

        if (rd_bytes.size() >= packet_len) {
            jdwp::handle_request((jdwp::CommandPacket*)rd_bytes.begin());
            consume(packet_len);
        }
    }
}



void update(Class* current_clz,
            const ClassFile::MethodInfo* current_mtd,
            u32 current_pc)
{
    switch (state) {
    case State::await_connection:
        if (connection::is_connected()) {
            // puts("debug loop accepted connection, state transition to "
            //      "handshake");
            state = State::handshake;
        }
        break;


    case State::handshake: {

        receive();

        if (rd_bytes.size() >= sizeof(jdwp::Handshake)) {
            jdwp::Handshake compare;
            if (memcmp(&compare, rd_bytes.begin(), sizeof compare) == 0) {
                // puts("validated handshake!");
                connection::send(&compare, sizeof compare);
                state = State::ready;
                // jdwp::notify_vm_start();
            } else {
                // puts("receive error");
                state = State::await_connection;
            }

            consume(sizeof(jdwp::Handshake));
        }
        break;
    }


    case State::ready: {

        auto br = active_breakpoints;
        while (br) {
            if (hit_breakpoint(*br, current_clz, current_mtd, current_pc)) {
                jdwp::notify_breakpoint_hit(current_clz,
                                            current_mtd,
                                            current_pc,
                                            br->req_);
                state = State::halted;
                goto HALT;
            }
            br = br->next_;
        }

        process_command();
        break;
    }


    case State::halted:
        HALT:
        while (true) {
            process_command();
        }
        break;
    }
}



static Breakpoint* create_breakpoint()
{
    if (free_breakpoints == nullptr) {
        return nullptr;
    } else {
        // Move from the freelist to the activelist
        auto br = free_breakpoints;
        free_breakpoints = br->next_;
        br->next_ = active_breakpoints;
        active_breakpoints = br;
        return br;
    }
}



bool register_breakpoint(Class* clz,
                         const ClassFile::MethodInfo* mtd,
                         u32 byte_offset,
                         u32 req)
{
    if (auto br = create_breakpoint()) {
        br->clz_ = clz;
        br->method_ = mtd;
        br->bytecode_offset_ = byte_offset;
        br->req_ = req;
        return true;
    }
    return false;
}



} // namespace debugger
} // namespace jvm
} // namespace java


#endif // JVM_ENABLE_DEBUGGING
