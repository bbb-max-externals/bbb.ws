#include "c74_min.h"
#include "../../bbb/pending_queue.hpp"

#pragma push_macro("NIL")
#undef NIL
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>
#include "../../bbb/receive_policy.hpp"
#pragma pop_macro("NIL")

#include <mutex>
#include <vector>
#include <string>
#include <atomic>

using namespace c74::min;

static std::string to_string(const symbol& s) {
    return std::string((const char*)s);
}

struct pending_message {
    enum type_t { text_msg, binary_msg, connected, disconnected, error } type;
    std::string data;
    std::vector<std::uint8_t> bytes;
    int close_code{};
};

class bbb_ws_client : public object<bbb_ws_client> {
public:
    MIN_DESCRIPTION{"WebSocket client (send and receive)"};
    MIN_TAGS{"websocket, network"};
    MIN_AUTHOR{"ISHII 2bit"};

    inlet<> input{this, "(anything) send message or connect/disconnect"};
    outlet<> message_out{this, "(anything) received messages"};
    outlet<> status_out{this, "(symbol) connection status"};

    attribute<symbol> url{this, "url", "",
        description{"WebSocket URL (ws:// or wss://)"}
    };

    attribute<bool> auto_connect{this, "auto_connect", true,
        description{"Auto-connect on load"}
    };

    attribute<int> reconnect_interval{this, "reconnect_interval", 5000,
        description{"Reconnect interval in ms (0 = disabled)"},
        range{0, 60000}
    };

    attribute<bool> binary{this, "binary", false,
        description{"Send mode: text (0) or binary (1)"}
    };

    attribute<int> max_message_size{this, "max_message_size", 16384,
        description{"Maximum received bytes per message (applies on next start/connect)"},
        range{1, 16384}
    };

    attribute<symbol> subprotocol{this, "subprotocol", "",
        description{"Sec-WebSocket-Protocol header"}
    };

    message<> connect_msg{this, "connect", "Connect to WebSocket server",
        MIN_FUNCTION {
            do_connect();
            return {};
        }
    };

    message<> disconnect_msg{this, "disconnect", "Disconnect from server",
        MIN_FUNCTION {
            do_disconnect();
            return {};
        }
    };

    message<> send_msg{this, "send", "Send text frame",
        MIN_FUNCTION {
            if(args.size() < 1) return {};
            std::string text = format_atoms(args);
            m_ws.send(text);
            return {};
        }
    };

    message<> send_bytes_msg{this, "send_bytes", "Send binary frame (list of ints)",
        MIN_FUNCTION {
            if(args.size() < 1) return {};
            std::string binary_data;
            for(const auto& a : args) {
                binary_data.push_back(static_cast<std::uint8_t>(static_cast<int>(a)));
            }
            m_ws.send(binary_data, true);
            return {};
        }
    };

    message<> anything_msg{this, "anything", "Send message (selector + args as text)",
        MIN_FUNCTION {
            std::string text = format_atoms(args);
            if(binary) {
                m_ws.send(text, true);
            }
            else {
                m_ws.send(text);
            }
            return {};
        }
    };

    bbb_ws_client() {
        ix::initNetSystem();
        m_init_timer.delay(0);
    }

    ~bbb_ws_client() {
        m_shutting_down = true;
        m_reconnect_timer.stop();
        m_ws.stop();
    }

private:
    ix::WebSocket m_ws;
    bbb::pending_queue<pending_message> m_pending;
    std::atomic<bool> m_shutting_down{false};
    // Main-thread generation: queued events from a stopped socket cannot restart it.
    std::uint64_t m_attempt{};
    std::uint64_t m_timer_attempt{};
    std::atomic<std::uint64_t> m_retry_attempt{};

    std::string m_connected_url;

    timer<timer_options::defer_delivery> m_init_timer{this,
        MIN_FUNCTION {
            if(auto_connect) {
                do_connect();
            }
            return {};
        }
    };

    timer<timer_options::defer_delivery> m_reconnect_timer{this,
        MIN_FUNCTION {
            if(bbb::should_retry(m_timer_attempt, m_attempt, reconnect_interval.get())) do_connect();
            return {};
        }
    };

    queue<> m_queue{this,
        MIN_FUNCTION {
            auto batch = m_pending.drain();
            if(batch.report_drops) status_out.send(symbol("queue_overflow"));
            for(const auto& msg : batch.messages) {
                switch(msg.type) {
                    case pending_message::text_msg: {
                        atoms text{symbol("text")};
                        for(unsigned char byte : msg.data) text.push_back(static_cast<int>(byte));
                        message_out.send(text);
                        break;
                    }
                    case pending_message::binary_msg: {
                        atoms byte_list;
                        for(auto b : msg.bytes) {
                            byte_list.push_back(static_cast<int>(b));
                        }
                        message_out.send(byte_list);
                        break;
                    }
                    case pending_message::connected:
                        status_out.send(symbol("connected"), symbol(msg.data));
                        break;
                    case pending_message::disconnected:
                    case pending_message::error: {
                        atoms status{symbol(msg.type == pending_message::error ? "error" : "disconnected"),
                                     atom(msg.close_code)};
                        for(unsigned char byte : msg.data) status.push_back(static_cast<int>(byte));
                        status_out.send(status);
                        break;
                    }
                }
            }
            const auto retry = m_retry_attempt.exchange(0);
            if(bbb::should_retry(retry, m_attempt, reconnect_interval.get())) {
                m_timer_attempt = retry;
                m_reconnect_timer.delay(reconnect_interval.get());
            }
            return {};
        }
    };

    void push_pending(pending_message&& msg) {
        if(m_shutting_down) return;
        m_pending.push(std::move(msg));
        m_queue.set();
    }

    static std::string format_atoms(const atoms& args) {
        std::string text;
        for(std::size_t i = 0; i < args.size(); i++) {
            if(i > 0) text += " ";
            if(args[i].a_type == c74::max::A_LONG) {
                text += std::to_string(static_cast<int>(args[i]));
            }
            else if(args[i].a_type == c74::max::A_FLOAT) {
                text += std::to_string(static_cast<double>(args[i]));
            }
            else if(args[i].a_type == c74::max::A_SYM) {
                text += to_string(args[i]);
            }
        }
        return text;
    }

    void do_connect() {
        m_reconnect_timer.stop();

        auto u = to_string(url);
        if(u.empty()) {
            cerr << "bbb.ws.client: url is empty" << endl;
            return;
        }

        const auto attempt = ++m_attempt;
        m_ws.stop();

        m_connected_url = u;
        bbb::configure_receive(m_ws, max_message_size.get());

        m_ws.setUrl(u);

        auto sp = to_string(subprotocol);
        if(!sp.empty()) {
            ix::WebSocketHttpHeaders headers;
            headers["Sec-WebSocket-Protocol"] = sp;
            m_ws.setExtraHeaders(headers);
        }
        else {
            m_ws.setExtraHeaders({});
        }

        m_ws.setOnMessageCallback([this, attempt](const ix::WebSocketMessagePtr& msg) {
            if(m_shutting_down) return;

            if(msg->type == ix::WebSocketMessageType::Message) {
                if(msg->binary) {
                    pending_message pm;
                    pm.type = pending_message::binary_msg;
                    pm.bytes.assign(msg->str.begin(), msg->str.end());
                    push_pending(std::move(pm));
                }
                else {
                    pending_message pm;
                    pm.type = pending_message::text_msg;
                    pm.data = msg->str;
                    push_pending(std::move(pm));
                }
            }
            else if(msg->type == ix::WebSocketMessageType::Open) {
                pending_message pm;
                pm.type = pending_message::connected;
                pm.data = m_connected_url;
                push_pending(std::move(pm));
            }
            else if(msg->type == ix::WebSocketMessageType::Close) {
                pending_message pm;
                pm.type = pending_message::disconnected;
                pm.close_code = msg->closeInfo.code;
                pm.data = msg->closeInfo.reason.substr(0, 256);
                push_pending(std::move(pm));

            }
            else if(msg->type == ix::WebSocketMessageType::Error) {
                pending_message pm;
                pm.type = pending_message::error;
                pm.data = msg->errorInfo.reason.substr(0, 256);
                pm.close_code = msg->errorInfo.http_status;
                push_pending(std::move(pm));
            }
            if(msg->type == ix::WebSocketMessageType::Close ||
               (msg->type == ix::WebSocketMessageType::Error &&
                m_ws.getReadyState() == ix::ReadyState::Closed)) {
                m_retry_attempt = attempt;
                m_queue.set();
            }
        });

        m_ws.start();
    }

    void do_disconnect() {
        m_reconnect_timer.stop();
        ++m_attempt;
        m_ws.stop();
    }
};

MIN_EXTERNAL(bbb_ws_client);
