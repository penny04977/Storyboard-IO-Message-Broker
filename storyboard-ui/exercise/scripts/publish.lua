-- scripts/publish.lua
local gre = gre   -- Storyboard gre API
local BROKER_CHANNEL = "broker_channel"  -- to broker.cpp
local UI_CHANNEL     = "ui_channel"      -- channel Storyboard listens on (plugin options -> greio)

-- Register this UI as a client with the broker.
-- Broker REGISTER handler expects:
--   name   = "register"
--   format = "1s0"
--   data   = <client_channel>
function register_ui_client(mapargs)
    local client_channel = UI_CHANNEL  -- e.g. "ui_channel"
    local data = {value = client_channel}

    local ok, err = gre.send_event_data(
        "register",
        "1s0 value",
        data,
        BROKER_CHANNEL
    )

    print("register_ui_client:", ok, err)
end

-- Subscribe this UI to a particular event/topic (e.g. "temperature").
-- Broker SUBSCRIBE handler expects:
--   name   = "subscribe"
--   format = "1s0"
--   data   = <event_name>
-- and uses `target` (client channel) internally.
function subscribe_ui_to_temperature(mapargs)
    local event_name    = "temperature"
    local data = { value = event_name }

    local ok, err = gre.send_event_data(
        "subscribe",
        "1s0 value",
        data,      -- payload: "temperature"
        BROKER_CHANNEL
    )

    print("subscribe_ui_to_temperature:", ok, err)
end

-- Publish an event to the broker (e.g. "temperature").
-- Broker PUBLISH handler expects:
--   name   = event name
--   format = "1s0"
--   data   = <value>
function publish_event(mapargs)
    -- Actual UI value, e.g. from a control. For now hard-code:
    local value = 28

    -- One string field named 'value'
    local data = { value = tostring(value) }  -- key 'value', string payload

    local ok, err = gre.send_event_data(
        "temperature",    -- event name
        "1s0 value",
        data,
        "broker_channel"
    )
    print("send_event_data:", ok, err)
end

-- Example: 
function on_temperature(mapargs)
    print("Received temperature:", mapargs.value)
end