#include "c74_min.h"

#pragma push_macro("NIL")
#undef NIL
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>
#pragma pop_macro("NIL")

#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <atomic>
#include <memory>

using namespace c74::min;

static std::string to_string(const symbol& s) {
	return std::string((const char*)s);
}

class bbb_ws_server : public object<bbb_ws_server> {
public:
	MIN_DESCRIPTION{"WebSocket server"};
	MIN_TAGS{"websocket, server, network"};
	MIN_AUTHOR{"ISHII 2bit"};

	inlet<> input{this, "(anything) broadcast or send to clients"};
	outlet<> message_out{this, "(anything) received messages from clients"};
	outlet<> status_out{this, "(symbol) connection events"};

	attribute<int> port{this, "port", 8080,
		description{"Listen port"},
		range{0, 65535}
	};

	attribute<symbol> bind_ip{this, "bind_ip", "0.0.0.0",
		description{"Bind interface"}
	};

	attribute<int> max_clients{this, "max_clients", 10,
		description{"Max concurrent connections"},
		range{1, 1000}
	};

	attribute<bool> tls_attr{this, "tls", false,
		description{"Enable WSS (TLS)"}
	};

	attribute<symbol> cert_file{this, "cert_file", "",
		description{"TLS certificate file path"}
	};

	attribute<symbol> key_file{this, "key_file", "",
		description{"TLS private key file path"}
	};

	attribute<symbol> subprotocol{this, "subprotocol", "",
		description{"Sec-WebSocket-Protocol"}
	};

	message<> start_msg{this, "start", "Start listening on @port",
		MIN_FUNCTION {
			start_server();
			return {};
		}
	};

	message<> stop_msg{this, "stop", "Stop the server",
		MIN_FUNCTION {
			stop_server();
			return {};
		}
	};

	message<> broadcast_msg{this, "broadcast", "Send to all connected clients",
		MIN_FUNCTION {
			if(args.size() < 1) return {};
			std::string text = format_atoms(args);
			broadcast(text);
			return {};
		}
	};

	message<> send_msg{this, "send", "Send to specific client: send <client_id> <message>",
		MIN_FUNCTION {
			if(args.size() < 2) {
				cerr << "bbb.ws.server: send requires <client_id> <message>" << endl;
				return {};
			}
			int client_id = static_cast<int>(args[0]);
			atoms rest(args.begin() + 1, args.end());
			std::string text = format_atoms(rest);
			send_to_client(client_id, text);
			return {};
		}
	};

	message<> close_client_msg{this, "close", "Disconnect a specific client: close <client_id>",
		MIN_FUNCTION {
			if(args.size() < 1) {
				cerr << "bbb.ws.server: close requires <client_id>" << endl;
				return {};
			}
			int client_id = static_cast<int>(args[0]);
			close_client(client_id);
			return {};
		}
	};

	message<> anything_msg{this, "anything", "Alias for broadcast",
		MIN_FUNCTION {
			std::string text = format_atoms(args);
			broadcast(text);
			return {};
		}
	};

	bbb_ws_server()
		: m_next_id(1) {
		ix::initNetSystem();
		m_init_timer.delay(0);
	}

	~bbb_ws_server() {
		m_shutting_down = true;
		stop_server();
	}

private:
	struct pending_message {
		enum type_t { client_text, client_binary, status } type;
		std::string data;
		std::vector<uint8_t> bytes;
		int client_id;
	};

	std::unique_ptr<ix::WebSocketServer> m_server;
	std::map<uintptr_t, int> m_ptr_to_id;
	std::map<int, std::weak_ptr<ix::WebSocket>> m_id_to_ws;
	std::mutex m_clients_mtx;
	std::atomic<int> m_next_id;
	std::atomic<bool> m_shutting_down{false};

	int m_max_clients_snapshot{10};

	std::vector<pending_message> m_pending;
	std::mutex m_pending_mtx;

	queue<> m_queue{this, MIN_FUNCTION {
		drain_pending();
		return {};
	}};

	timer<timer_options::defer_delivery> m_init_timer{this,
		MIN_FUNCTION {
			start_server();
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

	std::string format_atoms(const atoms& args) {
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

	void start_server() {
		stop_server();

		int port_val = static_cast<int>(port);
		auto ip = to_string(bind_ip);
		m_max_clients_snapshot = static_cast<int>(max_clients);
		m_server = std::make_unique<ix::WebSocketServer>(port_val, ip);

		if(static_cast<bool>(tls_attr)) {
			ix::SocketTLSOptions tls_options;
			tls_options.certFile = to_string(cert_file);
			tls_options.keyFile = to_string(key_file);
			tls_options.tls = true;
			m_server->setTLSOptions(tls_options);
		}

		m_server->setOnConnectionCallback(
			[this](std::weak_ptr<ix::WebSocket> ws_weak, std::shared_ptr<ix::ConnectionState>) {
				if(m_shutting_down) return;

				auto ws = ws_weak.lock();
				if(!ws) return;
				uintptr_t key = reinterpret_cast<uintptr_t>(ws.get());
				int id = m_next_id.fetch_add(1);
				{
				auto lock = std::lock_guard<std::mutex>(m_clients_mtx);
				if(static_cast<int>(m_id_to_ws.size()) >= m_max_clients_snapshot) {
						ws->close(1013, "Max clients reached");
						return;
					}
					m_ptr_to_id[key] = id;
					m_id_to_ws[id] = ws_weak;
				}
				push_pending({pending_message::status, "connected", {}, id});
			}
		);

		m_server->setOnClientMessageCallback(
			[this](std::shared_ptr<ix::ConnectionState>, ix::WebSocket& ws, const ix::WebSocketMessagePtr& msg) {
				if(m_shutting_down) return;

				uintptr_t key = reinterpret_cast<uintptr_t>(&ws);

				if(msg->type == ix::WebSocketMessageType::Message) {
					int id = 0;
					{
						auto lock = std::lock_guard<std::mutex>(m_clients_mtx);
						auto it = m_ptr_to_id.find(key);
						if(it == m_ptr_to_id.end()) return;
						id = it->second;
					}
					if(msg->binary) {
						std::vector<uint8_t> bytes(msg->str.begin(), msg->str.end());
						push_pending({pending_message::client_binary, "", bytes, id});
					}
					else {
						push_pending({pending_message::client_text, msg->str, {}, id});
					}
				}
				else if(msg->type == ix::WebSocketMessageType::Close) {
					int id = 0;
					{
						auto lock = std::lock_guard<std::mutex>(m_clients_mtx);
						auto it = m_ptr_to_id.find(key);
						if(it == m_ptr_to_id.end()) return;
						id = it->second;
						m_ptr_to_id.erase(it);
						m_id_to_ws.erase(id);
					}
					push_pending({pending_message::status, "disconnected", {}, id});
				}
			}
		);

		bool res = m_server->listenAndStart();
		if(!res) {
			cerr << "bbb.ws.server: failed to listen on " << ip << ":" << port_val << endl;
			return;
		}

		cout << "bbb.ws.server: listening on " << ip << ":" << port_val << endl;
	}

	void stop_server() {
		if(m_server) {
			m_server->stop();
		}
		{
			auto lock = std::lock_guard<std::mutex>(m_clients_mtx);
			m_ptr_to_id.clear();
			m_id_to_ws.clear();
		}
		cout << "bbb.ws.server: stopped" << endl;
	}

	void broadcast(const std::string& text) {
		std::vector<std::shared_ptr<ix::WebSocket>> targets;
		{
			auto lock = std::lock_guard<std::mutex>(m_clients_mtx);
			for(auto& [id, ws_weak] : m_id_to_ws) {
				auto ws = ws_weak.lock();
				if(ws) targets.push_back(std::move(ws));
			}
		}
		for(auto& ws : targets) {
			ws->send(text);
		}
	}

	void send_to_client(int client_id, const std::string& text) {
		std::shared_ptr<ix::WebSocket> target;
		{
			auto lock = std::lock_guard<std::mutex>(m_clients_mtx);
			auto it = m_id_to_ws.find(client_id);
			if(it == m_id_to_ws.end()) {
				cerr << "bbb.ws.server: client " << client_id << " not found" << endl;
				return;
			}
			target = it->second.lock();
		}
		if(target) {
			target->send(text);
		}
		else {
			cerr << "bbb.ws.server: client " << client_id << " disconnected" << endl;
		}
	}

	void close_client(int client_id) {
		std::shared_ptr<ix::WebSocket> ws_to_close;
		{
			auto lock = std::lock_guard<std::mutex>(m_clients_mtx);
			auto it = m_id_to_ws.find(client_id);
			if(it == m_id_to_ws.end()) {
				cerr << "bbb.ws.server: client " << client_id << " not found" << endl;
				return;
			}
			ws_to_close = it->second.lock();
			for(auto pit = m_ptr_to_id.begin(); pit != m_ptr_to_id.end(); ++pit) {
				if(pit->second == client_id) {
					m_ptr_to_id.erase(pit);
					break;
				}
			}
			m_id_to_ws.erase(it);
		}
		if(ws_to_close) {
			ws_to_close->close(1000, "Closed by server");
		}
	}

	void drain_pending() {
		std::vector<pending_message> msgs;
		{
			auto lock = std::lock_guard<std::mutex>(m_pending_mtx);
			msgs.swap(m_pending);
		}
		for(const auto& msg : msgs) {
			switch(msg.type) {
				case pending_message::client_text: {
					atoms a;
					a.push_back(atom(msg.client_id));
					a.push_back(atom(symbol(msg.data)));
					message_out.send(a);
					break;
				}
				case pending_message::client_binary: {
					atoms a;
					a.push_back(atom(msg.client_id));
					for(auto b : msg.bytes) {
						a.push_back(atom(static_cast<int>(b)));
					}
					message_out.send(a);
					break;
				}
				case pending_message::status: {
					atoms a;
					a.push_back(atom(symbol(msg.data)));
					a.push_back(atom(msg.client_id));
					status_out.send(a);
					break;
				}
			}
		}
	}
};

MIN_EXTERNAL(bbb_ws_server);
