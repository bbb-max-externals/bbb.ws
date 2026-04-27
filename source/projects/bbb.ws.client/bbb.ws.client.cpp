#include "c74_min.h"

#pragma push_macro("NIL")
#undef NIL
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>
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
    enum type_t { text_msg, binary_msg, status_msg, reconnect_msg } type;
    std::string data;
    std::vector<std::uint8_t> bytes;
    int close_code;
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
    std::vector<pending_message> m_pending;
    std::mutex m_pending_mtx;
    std::atomic<bool> m_shutting_down{false};

    std::string m_connected_url;
    int m_reconnect_interval_snapshot{0};

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
            do_connect();
            return {};
        }
    };

    queue<> m_queue{this,
        MIN_FUNCTION {
            std::vector<pending_message> msgs;
            {
                auto lock = std::lock_guard<std::mutex>(m_pending_mtx);
                msgs.swap(m_pending);
            }
            for(const auto& msg : msgs) {
                switch(msg.type) {
                    case pending_message::text_msg:
                        message_out.send(symbol(msg.data));
                        break;
                    case pending_message::binary_msg: {
                        atoms byte_list;
                        for(auto b : msg.bytes) {
                            byte_list.push_back(static_cast<int>(b));
                        }
                        message_out.send(byte_list);
                        break;
                    }
                    case pending_message::status_msg:
                        status_out.send(symbol(msg.data));
                        break;
                    case pending_message::reconnect_msg:
                        m_reconnect_timer.delay(msg.close_code);
                        break;
                }
            }
            return {};
        }
    };

    void push_pending(pending_message&& msg) {
        if(m_shutting_down) return;
        {
            auto lock = std::lock_guard<std::mutex>(m_pending_mtx);
            m_pending.push_back(std::move(msg));
        }
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

        m_ws.stop();

        m_connected_url = u;
        m_reconnect_interval_snapshot = static_cast<int>(reconnect_interval);

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

        m_ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
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
                pm.type = pending_message::status_msg;
                pm.data = "connected " + m_connected_url;
                push_pending(std::move(pm));
            }
            else if(msg->type == ix::WebSocketMessageType::Close) {
                pending_message pm;
                pm.type = pending_message::status_msg;
                pm.close_code = msg->closeInfo.code;
                pm.data = "disconnected " + std::to_string(msg->closeInfo.code)
                    + " " + msg->closeInfo.reason;
                push_pending(std::move(pm));

                if(0 < m_reconnect_interval_snapshot) {
                    pending_message rpm;
                    rpm.type = pending_message::reconnect_msg;
                    rpm.close_code = m_reconnect_interval_snapshot;
                    push_pending(std::move(rpm));
                }
            }
            else if(msg->type == ix::WebSocketMessageType::Error) {
                pending_message pm;
                pm.type = pending_message::status_msg;
                pm.data = "error " + msg->errorInfo.reason;
                push_pending(std::move(pm));
            }
        });

        m_ws.start();
    }

    void do_disconnect() {
        m_reconnect_timer.stop();
        m_ws.stop();
    }
};

MIN_EXTERNAL(bbb_ws_client);
