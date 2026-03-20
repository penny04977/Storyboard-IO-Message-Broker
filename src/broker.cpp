#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <pthread.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip> // Required for std::put_time
#include "protocol.h"

extern "C" {
#include "greio.h"
}

struct BrokerEvent {
    std::string client_channel;
    std::string event_name;
    std::string message;
};

std::unordered_map<
    std::string,                        // client channel
    std::unordered_set<std::string>     // subscribed event names
> subscribers;

// Last 10 messages for each event
std::unordered_map<std::string, std::deque<std::string>> event_history;

std::queue<BrokerEvent> msg_queue;

pthread_mutex_t sub_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hist_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  queue_cond  = PTHREAD_COND_INITIALIZER;

std::ofstream logfile("broker.log");

// ------------------------------------------------
// logging
// ------------------------------------------------
void log_line(const std::string &msg) {

    pthread_mutex_lock(&log_mutex);

    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    // Format: [Mon Mar 15 20:16:00 2026] (without the trailing \n)
    logfile << "[" << std::put_time(timeinfo, "%a %b %d %H:%M:%S %Y") << "] " 
            << msg << std::endl;

    logfile.flush();

    pthread_mutex_unlock(&log_mutex);
}

// ------------------------------------------------
// store a message to event_history
// pop the front when its size > 10
// ------------------------------------------------
void store_message(const std::string& event_name, const std::string& message) {

    pthread_mutex_lock(&hist_mutex);

    auto& history = event_history[event_name];

    history.push_back(message);
    /*
    std::cout << "[TRACE] Store message to event history: "
            << event_name << " - "
            << message << std::endl;    
    */
    if (history.size() > 10) {
        history.pop_front();
    }

    pthread_mutex_unlock(&hist_mutex);
}

// ------------------------------------------------
// send event to a client channel
// ------------------------------------------------
void send_to_client(const std::string &channel,
                    const std::string &event_name,
                    const std::string &message) {

    gre_io_t *handle = gre_io_open(channel.c_str(), GRE_IO_TYPE_WRONLY);
    if (!handle) {
        log_line("[ERROR] Failed to open channel to " + channel);               
        return;
    }

    gre_io_serialized_data_t *buffer = NULL;

    buffer = gre_io_serialize(
        buffer,
        channel.c_str(),
        event_name.c_str(),
        "1s0",
        message.c_str(),
        message.size() + 1
    );

    if (buffer) {
        gre_io_send(handle, buffer);
        gre_io_free_buffer(buffer);
        std::cout << "[TRACE] Send to client: "
                << event_name << " -> "
                << channel << std::endl;
                    
        log_line(std::string("[SEND] event=") + event_name + " data=" + message + " -> " + channel);
    }

    gre_io_close(handle);
}

// ------------------------------------------------
// broadcast message to subscribers
// ------------------------------------------------
void broadcast_message(const BrokerEvent &event) {

    pthread_mutex_lock(&sub_mutex);

    for (auto &client : subscribers) {
        const std::string &channel = client.first;
        const std::unordered_set<std::string> &events = client.second;

        for (auto &e : events) {
            if (e == event.event_name) {
                send_to_client(channel, event.event_name, event.message);
            }
        }
    }

    pthread_mutex_unlock(&sub_mutex);
}

// ------------------------------------------------
// Sending thread
// In this exercise, the broker runs indefinitely; no shutdown signal handling.
// ------------------------------------------------
void *send_thread(void *arg) {

    while (true) {

        pthread_mutex_lock(&queue_mutex);

        while (msg_queue.empty())
            pthread_cond_wait(&queue_cond, &queue_mutex);

        BrokerEvent event = msg_queue.front();

        msg_queue.pop();
        
        pthread_mutex_unlock(&queue_mutex);

        broadcast_message(event);
    }

    return NULL;
}

// ------------------------------------------------
// main
// ------------------------------------------------
int main() {

    gre_io_t *recv_handle;
    gre_io_t *ui_handle;
    pthread_t send;

    gre_io_serialized_data_t *recv_buffer = NULL;

    recv_handle = gre_io_open(BROKER_CHANNEL, GRE_IO_TYPE_RDONLY);
    if (!recv_handle) {
        std::cout << "Failed to open broker channel\n";
        return -1;
    }

    // From broker → Storyboard UI
    ui_handle = gre_io_open(UI_CHANNEL, GRE_IO_TYPE_RDONLY);
    if (!ui_handle) {
        std::cout << "Failed to open UI channel\n";
        return -1;
    }

    pthread_create(&send, NULL, send_thread, NULL);

    while (true) {

        char *target;
        char *name;
        char *format;
        void *data;

        int size = gre_io_receive(recv_handle, &recv_buffer);

        if (size <= 0)
            continue;

        gre_io_unserialize(recv_buffer, &target, &name, &format, &data);
        /*
        std::cout << "[TRACE] Raw recv: name=" << (name ? name : "(null)")
                << " format=" << (format ? format : "(null)") 
                << " data=" << (data ? (char*)data : "(null)")<< std::endl;
        */

        // -----------------------------------------
        // REGISTER
        // format: 1s0
        // channel
        // -----------------------------------------
        if (strcmp(name, REGISTER) == 0) {

            // "1s0 value" for Storyboard UI
            if ((strcmp(format, "1s0") == 0) ||
                (strcmp(format, "1s0 value") == 0)) {

                std::string channel((char *)data);

                pthread_mutex_lock(&sub_mutex);

                subscribers[channel];

                pthread_mutex_unlock(&sub_mutex);

                log_line(std::string("[RECV] event= ") + name + " data=" + (char*)data);
                std::cout << "Registered: "
                          << channel << std::endl;
            }
            else {
                std::cout << "[ERROR] Incorrect format for REGISTER: " << format << std::endl;
            }
        }

        // -----------------------------------------
        // SUBSCRIBE
        // format: 1s0
        // event
        // -----------------------------------------
        else if (strcmp(name, SUBSCRIBE) == 0) {

            // "1s0 value" for Storyboard UI
            if ((strcmp(format, "1s0") == 0) ||
                (strcmp(format, "1s0 value") == 0)) {

                std::string channel(target);
                std::string event((char*)data);

                pthread_mutex_lock(&sub_mutex);

                subscribers[channel].insert(event);

                // Print Suscriber Table
                std::cout << "\n[TRACE] Subscribers table:\n";

                for (const auto& pair : subscribers) {
                    const std::string& ch = pair.first;
                    const std::unordered_set<std::string>& events = pair.second;

                    std::cout << "  Channel: " << (ch.empty() ? "(EMPTY)" : ch) << " - [";

                    bool first = true;
                    for (const auto& e : events) {
                        if (!first) std::cout << ", ";
                        std::cout << e;
                        first = false;
                    }

                    std::cout << "]\n";
                }

                pthread_mutex_unlock(&sub_mutex);

                log_line(std::string("[RECV] event=") + name + " data=" + channel + " " + event);

                std::cout << "Subscribed: "
                    << channel << " - "
                    << event << std::endl;

                // Send history
                pthread_mutex_lock(&hist_mutex);

                auto it = event_history.find(event);

                if (it != event_history.end()) {

                    for (const auto& msg : it->second) {

                        send_to_client(channel, event, msg);

                        std::cout << "[TRACE] Send event history to: "
                          << channel << " - "
                          << event << std::endl;
                    }
                }
                pthread_mutex_unlock(&hist_mutex);
            }
            else {
                std::cout <<"[ERROR] Incorret format for Subscribe: " 
                        << format << std::endl;
            }
        }

        // -----------------------------------------
        // UNSUBSCRIBE
        // format: 1s0
        // event
        // -----------------------------------------
        else if (strcmp(name, UNSUBSCRIBE) == 0) {

            // "1s0 value" for Storyboard UI
            if ((strcmp(format, "1s0") == 0) ||
                (strcmp(format, "1s0 value") == 0)) {

                std::string channel(target);
                std::string event((char*)data);

                pthread_mutex_lock(&sub_mutex);

                subscribers[channel].erase(event);

                pthread_mutex_unlock(&sub_mutex);

                log_line(std::string("[RECV] event= ") + name + " data=" + channel + " " + event);

                std::cout << "Unsubscribed: "
                        << channel << " - "
                        << event << std::endl;
            }
            else {
                std::cout <<"[ERROR] Incorret format for Unsubscribe: " 
                        << format << std::endl;
            }
        }

        // -----------------------------------------
        // treat other events as PUBLISH
        // format: 1s0
        // message
        // -----------------------------------------
        else {
            // "1s0 value" for Storyboard UI
            if ((strcmp(format, "1s0") == 0) ||
                (strcmp(format, "1s0 value") == 0)) {

                std::string event_name(name);
                std::string message = std::string((char*)data);

                BrokerEvent event;
                event.event_name = event_name;
                event.message = message;

                pthread_mutex_lock(&queue_mutex);

                msg_queue.push(event);

                pthread_cond_signal(&queue_cond);

                pthread_mutex_unlock(&queue_mutex);

                store_message(event_name, message);

                log_line(std::string("[RECV] event=") + name + " data=" + message);

                std::cout << "Publish: "
                        << event_name << " - "
                        << message << std::endl;
            }
            else {
                std::cout <<"[ERROR] Incorret format for Publish: " 
                        << format << std::endl;
            }
        }
    }

    gre_io_close(recv_handle);

    return 0;
}