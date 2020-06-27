#pragma once

#include "zmq.hpp"

#include <functional>
#include <future>
#include <string>


namespace addr
{
	static const std::string MSG_BUS_IN = "inproc://bus-in";
	static const std::string MSG_BUS_OUT = "inproc://bus-out";
	static const std::string MSG_BUS_CONTROL = "inproc://bus-control";
}

namespace msg
{
	using on_ready_fn = std::function<void()>;
	using notifier_thread = std::function<void(std::shared_ptr<zmq::context_t>, on_ready_fn)>;

	// TODO: Shorten all messages since ZMQ uses prefix matching? Or even just use ints!

#ifdef _DEBUG
	static const std::string TEST = "TEST";
#endif

	// Connection messages - no data
	static const std::string READY = "READY";
	static const std::string EXIT = "EXIT";
	static const std::string BUS_CREATED = "BUS_CREATED";
	static const std::string CONNECTED_TO_BUS = "CONNECTED_TO_BUS";
	static const std::string START = "START";

	// Messages with exactly one receiver (usually comes with some heap data)
	static const std::string MESH_GEN_REQUEST = "MESH_GEN_REQUEST";
	static const std::string MESH_GEN_RESPONSE = "MESH_GEN_RESPONSE";
	static const std::string CHUNK_GEN_REQUEST = "CHUNK_GEN_REQUEST";
	static const std::string CHUNK_GEN_RESPONSE = "CHUNK_GEN_RESPONSE";
	static const std::string MINI_GET_REQUEST = "MINI_GET_REQUEST";
	static const std::string MINI_GET_RESPONSE = "MINI_GET_RESPONSE";

	// Messages with multiple receivers (every recipent gets a copy of the data)
	static const std::string EVENT_PLAYER_MOVED_CHUNKS = "EVENT_PLAYER_MOVED_CHUNKS";


	const std::vector<std::string> meshing_thread_incoming = {
		msg::EXIT,
		msg::MESH_GEN_REQUEST,
		msg::MINI_GET_RESPONSE,
		EVENT_PLAYER_MOVED_CHUNKS
	};

	const std::vector<std::string> chunk_gen_thread_incoming = {
		msg::EXIT,
		msg::CHUNK_GEN_REQUEST,
		EVENT_PLAYER_MOVED_CHUNKS
	};

	const std::vector<std::string> world_thread_incoming = {
		msg::EXIT,
		msg::MINI_GET_REQUEST,
		msg::CHUNK_GEN_RESPONSE
	};

	const std::vector<std::string> render_thread_incoming = {
		msg::EXIT,
		msg::MESH_GEN_RESPONSE
	};


	// zmq_proxy_steerable messages BEGIN
	static const std::string PAUSE = "PAUSE";
	static const std::string RESUME = "RESUME";
	static const std::string TERMINATE = "TERMINATE";
	// zmq_proxy_steerable messages END

	std::string gen_unique_addr();
	std::future<void> launch_thread_wait_until_ready(std::shared_ptr<zmq::context_t> ctx, notifier_thread thread);
}

class BusNode
{
public:
	BusNode(std::shared_ptr<zmq::context_t> ctx_);

	zmq::socket_t in;
	zmq::socket_t out;

private:
	std::shared_ptr<zmq::context_t> ctx;
};
