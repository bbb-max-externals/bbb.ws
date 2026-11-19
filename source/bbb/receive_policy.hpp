#pragma once

#include <algorithm>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

namespace bbb {
// Leave room for the text selector and client ID in a Max atom list.
constexpr int max_payload_bytes = 16384;

inline bool should_retry(std::uint64_t event_attempt, std::uint64_t current_attempt, int interval_ms) {
    return event_attempt != 0 && event_attempt == current_attempt && 0 < interval_ms;
}

inline void configure_receive(ix::WebSocket& socket, int bytes) {
    socket.setMaxMessageSize(std::clamp(bytes, 1, max_payload_bytes));
    socket.disableAutomaticReconnection(); // The Max timer is the only retry owner.
}

inline void configure_receive(ix::WebSocketServer& server, int bytes) {
    server.setMaxMessageSize(std::clamp(bytes, 1, max_payload_bytes));
}
}
