#include <iostream>
#include <cstring>
#include "protocol.h"

extern "C" {
#include "greio.h"
}

int main() {

    gre_io_t *handle;
    gre_io_serialized_data_t *buffer = NULL;

    handle = gre_io_open(BROKER_CHANNEL, GRE_IO_TYPE_WRONLY);

    if (!handle) {
        std::cout << "Failed to open channel to broker\n";
        return -1;
    }

    // In this exercise, the publisher runs indefinitely; no shutdown signal handling.
    while (true) {

        std::string event;
        std::string message;

        std::cout << "Event name: ";
        std::cin >> event;

        std::cout << "Message: ";
        std::cin >> message;

        char data[256];
        if (message.size() >= sizeof(data)) {
            std::cerr << "Message too long\n";
            continue;
        }
        strcpy(data, message.c_str());

        buffer = gre_io_serialize(
            buffer,
            BROKER_CHANNEL,
            event.c_str(),
            "1s0",
            data,
            message.size() + 1
        );

        if (buffer) {
            gre_io_send(handle, buffer);
            gre_io_free_buffer(buffer);
            buffer = NULL; 
        }
    }

    gre_io_close(handle);
}