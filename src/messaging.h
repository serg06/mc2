#pragma once

#include <functional>
#include <future>
#include <iostream>
#include <string>

#include "util.h"
#include "zmq_addon.hpp"

namespace addr
{
	static const std::string MSG_BUS_IN = "inproc://bus-in";
	static const std::string MSG_BUS_OUT = "inproc://bus-out";
	static const std::string MSG_BUS_CONTROL = "inproc://bus-control";
}

namespace msg
{
	using on_ready_fn = std::function<void()>;
	using notifier_thread = std::function<void(zmq::context_t*, on_ready_fn)>;

	// TODO: Shorten all of these since messages use prefix matching. Maybe even just use ints?
	static const std::string READY = "READY";
	static const std::string EXIT = "EXIT";
	static const std::string BUS_CREATED = "BUS_CREATED";
	static const std::string CONNECTED_TO_BUS = "CONNECTED_TO_BUS";
	static const std::string START = "START";
	static const std::string MESH_GEN_REQUEST = "MESH_GEN_REQUEST";
	static const std::string MESH_GEN_RESPONSE = "MESH_GEN_RESPONSE";
	static const std::string CHUNK_GEN_REQUEST = "CHUNK_GEN_REQUEST";
	static const std::string CHUNK_GEN_RESPONSE = "CHUNK_GEN_RESPONSE";
	static const std::string MINI_GET_REQUEST = "MINI_GET_REQUEST";
	static const std::string MINI_GET_RESPONSE = "MINI_GET_RESPONSE";
	static const std::string TEST = "TEST";

	// zmq_proxy_steerable messages BEGIN
	static const std::string PAUSE = "PAUSE";
	static const std::string RESUME = "RESUME";
	static const std::string TERMINATE = "TERMINATE";
	// zmq_proxy_steerable messages END

	// Convert multipart msg to string
	inline std::string multi_to_str(const std::vector<zmq::message_t>& resp)
	{
		if (resp[0].to_string_view() == "TEST")
		{
			int i = 0;
			i++;
		}
		std::stringstream s;
		s << "[" << resp[0].to_string_view() << "]: [";
		for (int i = 1; i < resp.size(); i++)
		{
			std::string_view sv = resp[i].to_string_view();
			bool printable = true;
			for (auto c : sv)
			{
				if (c < 32 || c > 126)
				{
					printable = false;
				}
			}
			if (printable)
			{
				s << sv;
			}
			else
			{
				s << "<binary data>";
			}
			if (i != resp.size() - 1)
			{
				s << ", ";
			}
		}
		s << "]";
		return s.str();
	}

	inline std::string gen_unique_addr()
	{
		// Not actually that random!
		static std::atomic_uint64_t extra(0);
		std::string suffix = std::to_string(extra.fetch_add(1)); // TODO: better memory order
		std::stringstream out;
		out << "inproc://unique-" << suffix;
		return out.str();
	}

	inline std::future<void> launch_thread_wait_until_ready(zmq::context_t* const ctx, notifier_thread thread)
	{
		// Listen to bus start
		std::string unique_addr = gen_unique_addr();
		zmq::socket_t pull(*ctx, zmq::socket_type::pull);
		pull.bind(unique_addr);

		on_ready_fn on_complete = [&]() {
			zmq::socket_t push(*ctx, zmq::socket_type::push);
			push.connect(unique_addr);
			push.send(zmq::buffer(msg::READY));
		};

		// Launch
		std::future<void> result = std::async(std::launch::async, thread, ctx, on_complete);

		// Wait for response
		std::vector<zmq::message_t> recv_msgs;
		auto ret = zmq::recv_multipart(pull, std::back_inserter(recv_msgs));
		assert(ret);
		assert(recv_msgs[0].to_string_view() == msg::READY);

		// Done
		return result;
	}

	//std::future<void> run_message_bus(zmq::context_t& ctx);
}

class BusNode
{
public:
	BusNode(zmq::context_t* const ctx_);
	
	zmq::socket_t in;
	zmq::socket_t out;

private:
	zmq::context_t* const ctx;
};
