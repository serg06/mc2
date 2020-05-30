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
	extern zmq::context_t ctx;

	using on_ready_fn = std::function<void()>;
	using notifier_thread = std::function<void(zmq::context_t*, on_ready_fn)>;

	static const std::string READY = "READY";
	static const std::string EXIT = "EXIT";
	static const std::string BUS_CREATED = "BUS_CREATED";
	static const std::string CONNECTED_TO_BUS = "CONNECTED_TO_BUS";
	static const std::string START = "START";
	static const std::string MESH_GEN_REQUEST = "MESH_GEN_REQUEST";
	static const std::string MESH_GEN_RESPONSE = "MESH_GEN_RESPONSE";
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

	inline std::future<void> launch_thread_wait_until_ready(notifier_thread thread)
	{
		// Listen to bus start
		std::string unique_addr = gen_unique_addr();
		zmq::socket_t pull(ctx, zmq::socket_type::pull);
		pull.bind(unique_addr);

		on_ready_fn on_complete = [&]() {
			zmq::socket_t push(ctx, zmq::socket_type::push);
			push.connect(unique_addr);
			push.send(zmq::buffer(msg::READY));
			push.close();
		};

		// Launch
		std::future<void> result = std::async(std::launch::async, thread, &ctx, on_complete);

		// Wait for response
		std::vector<zmq::message_t> recv_msgs;
		auto ret = zmq::recv_multipart(pull, std::back_inserter(recv_msgs));
		assert(ret);
		assert(recv_msgs[0].to_string_view() == msg::READY);
		pull.close();

		// Done
		return result;
	}

	//std::future<void> run_message_bus(zmq::context_t& ctx);
}
//
//int main()
//{
//	// Create context
//	zmq::context_t ctx(0);
//
//	// Listen to bus start
//	zmq::socket_t pull(ctx, zmq::socket_type::pull);
//	pull.bind("inproc://bus-status");
//
//	// Start bus
//	auto thread1 = std::async(std::launch::async, BusProxy, &ctx);
//
//	// Wait for it to start
//	zmq::message_t msg;
//	auto ret = pull.recv(msg);
//	assert(ret && msg.to_string_view() == msg::BUS_CREATED);
//
//	// Wew!
//	OutputDebugString("Successfully launched bus!\n");
//
//	// Launch all threads one at a time
//	std::vector<std::future<void>> meshers;
//	for (int i = 0; i < 1; i++)
//	{
//		meshers.push_back(launch_thread_wait_until_ready(ctx, pull, MeshGenThread));
//	}
//
//	// Create senders and listeners
//	std::vector<std::future<void>> listeners;
//	std::vector<std::future<void>> senders;
//	for (int i = 0; i < 1; i++)
//	{
//		listeners.push_back(launch_thread_wait_until_ready(ctx, pull, BusListener));
//		senders.push_back(launch_thread_wait_until_ready(ctx, pull, BusSender));
//	}
//
//	OutputDebugString("Launched listeners.\n");
//
//	return 0;
//}
