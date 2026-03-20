Storyboard IO Message Broker
1. Overview

This project implements a simple publish–subscribe message broker using Crank Storyboard IO and a C++ backend (including a publisher and a subscriber) and a Storyboard UI application that acts as a client.
There are four main components:
* broker.cpp
    * A multithreaded C++ message broker using greio (greio.h) and pthread.
    * Supports the following protocol:
        * register: register a client channel.
        * subscribe: subscribe a client channel to an event.
        * unsubscribe: unsubscribe a client channel from an event.
        * Any other event name is treated as publish: message is queued and broadcast to all subscribers of that event.
    * For each publish:
        * Stores the message in event_history (up to 10 per event).
        * Pushes a BrokerEvent into a queue.
        * A dedicated sending thread broadcasts the message to all subscribers using send_to_client.
	* Maintains:
        * A subscriber table: std::unordered_map<client_channel, std::unordered_set<event_name>>.
        * Per-event history: last 10 messages per event (event_history).
        * A message queue ‎`msg_queue` for published events that are pending broadcast.
    * Is explicitly designed to be thread‑safe for concurrent client subscriptions and message publishing:
        - Uses separate mutexes to protect each shared resource:
            * ‎`sub_mutex` for the ‎`subscribers` table (register/subscribe/unsubscribe).
            * ‎`hist_mutex` for ‎`event_history` (storing and replaying last messages).
            * ‎`queue_mutex` for ‎`msg_queue` (producer/consumer handoff).
            * ‎`log_mutex` for serialized file logging.
        - Uses a condition variable ‎`queue_cond` to coordinate between:
            * The main broker thread (producer) that pushes ‎`BrokerEvent` objects into ‎`msg_queue` on each publish.
            * A dedicated sending thread (consumer) that waits on ‎`queue_cond`, pops messages from the queue, and calls 					‎`broadcast_message` to deliver them to all subscribers.
        - All reads and writes to shared data structures go through these locks, preventing race conditions and ensuring data 				integrity when multiple clients are registering, subscribing, and publishing at the same time.
    * Logs activity to both stdout and broker.log (with timestamps).

* subscriber.cpp
    * A C++ console client that:
        * Prompts for a client channel name (e.g. client1).
        * Opens:
            * A receive channel on that name (to get events from broker).
            * A send handle to broker_channel (to send control messages).
        * Registers itself with the broker:
            * Sends register with format 1s0 and payload <client_channel>.
        * Spawns a receive thread that:
            * Calls gre_io_receive on the client channel.
            * Prints any incoming events and payloads.
        * Provides a simple command loop:
            * subscribe <event> → sends subscribe (1s0, payload <event>).
            * unsubscribe <event> → sends unsubscribe (1s0, payload <event>).
            * quit → exits the client.
    * Each subscriber process represents one client channel. To simulate multiple clients you start multiple subscriber processes in separate terminals.

* publisher.cpp
    * A C++ console publisher that:
        * Opens a send handle to BROKER_CHANNEL (UI/broker’s input channel).
        * In a loop:
            * Prompts for Event name: (e.g. temperature).
            * Prompts for Message.
            * Sends the event to broker_channel with format 1s0 and payload <message>.
    * This provides a simple non-UI publisher to test the broker and subscribers.

* Storyboard UI application
    * A simple UI built in Crank Storyboard Designer.
    * Uses Lua scripts to:
        * Register itself with the broker.
        * Publish temperature messages to the broker via Storyboard IO.
    * Communicates via:
        * broker_channel (UI → broker)
        * ui_channel (broker → UI; configured in the Storyboard greio plugin options).

2. Setup Instructions

2.1. Prerequisites
* Crank Storyboard Designer and Engine installed (with Storyboard IO / greio support).
* A C++ toolchain (e.g. g++) with:
    * POSIX threads (-pthread).
    * Access to the Storyboard IO headers and libraries:
        * greio.h
        * libgreio (name and path depend on your installation).
* A Storyboard workspace containing the UI project and Lua scripts.
You will need the appropriate include and library paths for greio.h and the greio library from your Storyboard installation. 

2.2. Building the C++ Components
All three C++ programs are built using a single `Makefile`.
From the project root (the directory containing `Makefile`, `project-include/`, and `src/`), run:make
This will:
* Compile `src/broker.cpp`, `src/subscriber.cpp`, and `src/publisher.cpp`
* Link them against the Storyboard IO (`greio`) library and `pthread`
* Produce three executables in the project root:.
./broker
./subscriber
./publisher

2.3. Configuring the Storyboard UI
In your Storyboard project:
*  IO Channels
    * In the Storyboard Simulator Configurations → plugin options → greio:
        * Set the application’s receive channel to:
            * ui_channel
    * This is the channel on which the UI receives messages from the broker.

*  Lua scripts for IO
    * Ensure your Lua scripts use:
        * BROKER_CHANNEL = "broker_channel" for sending from UI → broker.
        * UI_CHANNEL = "ui_channel" for the UI’s own receive channel.
    * Typical publish call (from a button callback):local BROKER_CHANNEL = "broker_channel" 

*  Event / IO Mapping
    * Map incoming events on ui_channel to Lua callbacks in the IO configuration.

2.4. Running the Demo

A typical manual test flow uses multiple terminals.

I.  Start the broker
In Terminal 1:
./broker  
Expected on startup:
* Opens BROKER_CHANNEL for reading.
* Spawns the sending thread.
* Waits for events.

When a client registers, you should see:
> Registered: client1
 
When a client subscribes to an event, you should see:
> Subscribed: client1 - button

> Send event history to: client1 - button
 
When an event is published, you should see:
> Publish: button - press 

II.  Start a subscriber client
In Terminal 2:
./subscriber  
When prompted:
Enter client channel: client1  
The client will:
* Open client1 as a receive channel (for broker → client1 events).
* Open a send handle to broker_channel.
* Send a register event to the broker.
* Start printing any events it receives.
You can then use the subscriber’s command loop:
Commands: subscribe <event>, unsubscribe <event>, quit  
> subscribe temperature  
> Subscribed to temperature 

III.  Start the publisher
In Terminal 3:
./publisher  
Then, for example:
> Event name: temperature  
> Message: 28  
The publisher sends temperature with payload "28" to broker_channel. The broker:
* Treats it as a PUBLISH.
* Stores the message in history.
* Broadcasts it to all subscribers for temperature, including client1.
In the subscriber terminal you should see:
[EVENT] temperature : 28

IV.  Run the Storyboard UI
In Storyboard Designer:
* Launch the Storyboard Simulator using the configuration with:
    * ui_channel as the application’s greio channel.
* Inside the UI:
    * Run exercise.gde - Simulator
    * Press the button “Register” to trigger the “register” action.
    * Press the button “Publish” to trigger “publish temperature” from the UI.
    
	The broker will treat the UI just like any other client:
* register and subscribe from the UI are handled by broker.cpp.
* temperature publishes from the UI are broadcast to all subscribers (including C++ subscribers).

3. Notes / Limitations
* One client channel per subscriber process
    subscriber.cpp is intentionally simple: each process manages exactly one client channel. To simulate multiple clients, start multiple subscriber processes with different channel names.

* Message format compatibility:
	To interoperate with both C++ clients and the Storyboard UI:
    * The broker accepts both “1s0”and “1s0 value” formats for control and publish messages.
    * C++ publisher/subscribers currently use “1s0”.
    * The Storyboard UI uses “1s0 value” for payloads defined in the Event Editor.

* UI lacking subscribe/unsubscribe:
    While the broker fully supports dynamic subscriptions for C++ clients, the UI client does not implement subscribe/unsubscribe and receiving logic because there are issues on message parsing. This might due to the reason that gre_io messaging API does not provide sender identity when receiving messages and need further investigation.

* Parsing and format:
	Some edge cases around payload parsing and format strings may still need refinement (especially when mixing 		different producers).

* Shutdown behavior:
    All three programs run indefinitely until terminated (or until quit in the subscriber). There is no signal handling or graceful shutdown logic beyond closing channels on normal exit.
