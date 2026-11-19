#pragma once

#include <climits>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

namespace bbb {
enum class text_mode { symbol, bytes, enum_count };
constexpr int default_message_bytes = 1024 * 1024;
constexpr int maximum_message_bytes = 64 * default_message_bytes;

// Max counts arguments in a signed short. A leading selector is not an argument.
constexpr bool fits_byte_output(std::size_t bytes, bool server, bool text) {
    const std::size_t prefix_atoms = server ? (text ? 2 : 1) : 0;
    return bytes <= static_cast<std::size_t>(SHRT_MAX) - prefix_atoms;
}

inline bool should_retry(std::uint64_t event_attempt, std::uint64_t current_attempt, int interval_ms) {
    return event_attempt != 0 && event_attempt == current_attempt && 0 < interval_ms;
}

inline void configure_receive(ix::WebSocket& socket, int bytes) {
    socket.setMaxMessageSize(bytes);
    socket.disableAutomaticReconnection(); // The Max timer is the only retry owner.
}

inline void configure_receive(ix::WebSocketServer& server, int bytes) {
    server.setMaxMessageSize(bytes);
}
}
