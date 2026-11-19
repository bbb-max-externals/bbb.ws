#include "../source/bbb/pending_queue.hpp"
#include "../source/bbb/receive_policy.hpp"
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXNetSystem.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

static_assert(bbb::fits_byte_output(32767, false, false) && !bbb::fits_byte_output(32768, false, false));
static_assert(bbb::fits_byte_output(32767, false, true) && !bbb::fits_byte_output(32768, false, true));
static_assert(bbb::fits_byte_output(32766, true, false) && !bbb::fits_byte_output(32767, true, false));
static_assert(bbb::fits_byte_output(32765, true, true) && !bbb::fits_byte_output(32766, true, true));

bool test_queue() {
    bbb::pending_queue<int> queue;
    const auto now = decltype(queue)::clock::now();
    auto fill = [&] {
        for(int i = 0; i <= static_cast<int>(queue.capacity); ++i) queue.push(i);
    };
    fill();
    auto batch = queue.drain(now);
    if(batch.messages.size() != queue.capacity || batch.messages.front() != 1 ||
       batch.messages.back() != queue.capacity || !batch.report_drops) return false;
    fill();
    return !queue.drain(now).report_drops && queue.drain(now + 1s).report_drops;
}

// Exchange ordinary text on loopback with intentionally small, asymmetric limits.
bool test_limit(bool server_limit) {
    std::atomic<int> opens{0}, delivered{0}, close_code{0};
    const auto port = ix::getFreePort();
    ix::WebSocketServer server(port, "127.0.0.1");
    bbb::configure_receive(server, server_limit ? 4 : 8);
    server.setOnClientMessageCallback([&](auto, ix::WebSocket& peer, const auto& event) {
        if(event->type == ix::WebSocketMessageType::Message) {
            peer.sendText(event->str == "ok" ? "ok" : "hello");
        }
    });
    if(!server.listenAndStart()) return false;
    ix::WebSocket client;
    bbb::configure_receive(client, server_limit ? 8 : 4);
    client.setUrl("ws://127.0.0.1:" + std::to_string(port));
    client.setOnMessageCallback([&](const auto& event) {
        if(event->type == ix::WebSocketMessageType::Open) { ++opens; client.sendText("ok"); }
        if(event->type == ix::WebSocketMessageType::Message) {
            ++delivered;
            if(event->str == "ok") client.sendText(server_limit ? "hello" : "next");
        }
        if(event->type == ix::WebSocketMessageType::Close) close_code = event->closeInfo.code;
    });
    client.start();
    const auto until = std::chrono::steady_clock::now() + 5s;
    while(!close_code && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(10ms);
    const bool closed = close_code == 1009;
    std::this_thread::sleep_for(200ms);
    client.stop();
    server.stop();
    return closed && delivered == 1 && opens == 1 && !client.isAutomaticReconnectionEnabled();
}

int main() {
    ix::initNetSystem();
    const bool transport_budget = [] {
        ix::WebSocket client;
        ix::WebSocketServer server(0, "127.0.0.1");
        bbb::configure_receive(client, bbb::default_message_bytes);
        bbb::configure_receive(server, bbb::maximum_message_bytes);
        return client.getMaxMessageSize() == 1048576 && server.getMaxMessageSize() == 67108864;
    }();
    const bool queue = test_queue(), server = test_limit(true), client = test_limit(false);
    ix::uninitNetSystem();
    std::cout << "queue=" << queue << " server-limit=" << server << " client-limit=" << client << '\n';
    const bool retry = !bbb::should_retry(1, 1, 0) && !bbb::should_retry(1, 2, 100) &&
                       bbb::should_retry(2, 2, 100);
    return queue && server && client && retry && transport_budget ? 0 : 1;
}
