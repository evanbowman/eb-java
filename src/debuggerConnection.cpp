
#if JVM_ENABLE_DEBUGGING


#include "debuggerConnection.hpp"
#include <SFML/Network.hpp>
//#include <iostream>
#include <mutex>
#include <thread>



namespace java {
namespace jvm {
namespace debugger {
namespace connection {



static Buffer current_buffer;
static u32 current_buffer_count;
static std::mutex lock;
static bool connected;
static sf::TcpSocket client;



void send(const void* data, u32 len)
{
    std::lock_guard<std::mutex> guard(lock);
    if (not connected) {
        return;
    }

    client.setBlocking(true);
    client.send(data, len);
    client.setBlocking(false);
}



void init(Buffer in)
{
    current_buffer = in;

    std::thread reader([] {
        sf::TcpListener listener;

        listener.listen(55004);
        if (listener.accept(client) == sf::Socket::Done) {
            std::cout << "debug client connected!" << std::endl;
            connected = true;
        }

        client.setBlocking(false);

        int inactive = 0;

        while (true) {

            if (inactive == 128) {
                inactive = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            std::lock_guard<std::mutex> guard(lock);
            std::size_t received;
            client.receive(current_buffer + current_buffer_count,
                           buffer_size - current_buffer_count,
                           received);

            current_buffer_count += received;
            if (received) {
                std::cout << "read " << received << " into buffer, remaining "
                          << buffer_size - current_buffer_count << std::endl;
            } else {
                ++inactive;
            }
        }
    });

    reader.detach();
}



bool is_connected()
{
    return connected;
}



u32 receive(Buffer in, Buffer* out)
{
    std::lock_guard<std::mutex> guard(lock);
    *out = current_buffer;
    current_buffer = in;
    auto ret = current_buffer_count;
    current_buffer_count = 0;
    return ret;
}



} // namespace connection
} // namespace debugger
} // namespace jvm
} // namespace java


#endif // JVM_ENABLE_DEBUGGING
