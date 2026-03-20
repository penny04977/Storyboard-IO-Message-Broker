#include <iostream>
#include <string>
#include <pthread.h>
#include <cstring>
#include "protocol.h"

extern "C" {
#include "greio.h"
}

gre_io_t *recv_handle = nullptr;
gre_io_t *send_handle = nullptr;

std::string client_channel;

/* ---------- RECEIVE THREAD ---------- */
void* receive_thread(void *arg)
{
    gre_io_serialized_data_t *buffer = NULL;

    char *target;
    char *name;
    char *format;
    void *data;

    // In this exercise, the subscriber runs indefinitely; no shutdown signal handling.
    while (true) {
        int size = gre_io_receive(recv_handle, &buffer);
        if (size <= 0)
            continue;

        gre_io_unserialize(buffer, &target, &name, &format, &data);

        std::cout << "\n[EVENT] " << name;

        if (data)
            std::cout << " : " << (char*)data;

        std::cout << std::endl;
    }

    return NULL;
}

/* ---------- SEND HELPER ---------- */
void send_event(const std::string &channel,
                const std::string &event_name,
                const std::string &format,
                const void *data,
                int data_size)
{
    gre_io_serialized_data_t *buffer = NULL;

    buffer = gre_io_serialize(
        buffer,
        channel.c_str(),
        event_name.c_str(),
        format.c_str(),
        data,
        data_size
    );

    if (buffer) {
        gre_io_send(send_handle, buffer);
        gre_io_free_buffer(buffer);
    }
}

/* ---------- MAIN ---------- */
int main()
{
    pthread_t recv_thread;

    std::cout << "Enter client channel: ";
    std::cin >> client_channel;

    /* open receive channel first */
    recv_handle = gre_io_open(client_channel.c_str(), GRE_IO_TYPE_RDONLY);

    if (!recv_handle) {
        std::cout << "Failed to open receive channel\n";
        return -1;
    }

    /* open broker channel for sending */
    send_handle = gre_io_open(BROKER_CHANNEL, GRE_IO_TYPE_WRONLY);

    if (!send_handle) {
        std::cout << "Failed to open broker channel\n";
        return -1;
    }

    /* start receive thread */
    pthread_create(&recv_thread, NULL, receive_thread, NULL);

    /* register with broker */
    send_event(client_channel, "register", "1s0", client_channel.c_str(), client_channel.size() + 1);

    std::cout << "Registered as " << client_channel << std::endl;

    /* command loop */
    while (true) {

        std::string cmd;
        std::string event;

        std::cout << "\nCommands: subscribe <event>, unsubscribe <event>, quit\n> ";
        std::cin >> cmd;

        if (cmd == "subscribe" || cmd == "unsubscribe") {

            std::cin >> event;

            // Prepare 1s0 payload: event\0
            char data[256];
            if (event.size() >= sizeof(data)) {
                std::cerr << "Event name too long\n";
                continue;
            }
            std::strcpy(data, event.c_str());

            send_event(client_channel, cmd, "1s0", data, event.size() + 1);

            std::cout << (cmd == "subscribe" ? "Subscribed to " : "Unsubscribed from ") << event << std::endl;
        }
        else if (cmd == "quit") {
            std::cout << "Exiting...\n";
            break;
        }
    }

    gre_io_close(recv_handle);
    gre_io_close(send_handle);

    return 0;
}