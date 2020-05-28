#include <functional>
#include <future>
#include <iostream>
#include <string>

#include "util.h"
#include "zmq_addon.hpp"

namespace addr
{
	// TODO: store common addresses here
}

namespace msg
{
	extern zmq::context_t ctx;

	using notifier_thread = std::function<void(zmq::context_t*, std::function<void()>)>;

	const std::string READY = "READY";
	const std::string BUS_CREATED = "BUS_CREATED";
	const std::string CONNECTED_TO_BUS = "CONNECTED_TO_BUS";
	const std::string START = "START";
	const std::string MESH_GEN_REQUEST = "MESH_GEN_REQUEST";
	const std::string MESH_GEN_RESPONSE = "MESH_GEN_RESPONSE";

	// Convert multipart msg to string
	static std::string multi_to_str(const std::vector<zmq::message_t>& resp)
	{
		std::stringstream s;
		s << "[" << resp[0].to_string_view() << "]: [";
		for (int i = 1; i < resp.size(); i++)
		{
			s << resp[i].to_string_view();
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

	inline std::future<void> launch_thread_wait_until_ready(zmq::context_t& ctx, zmq::socket_t& listener, notifier_thread thread)
	{
		// Listen to bus start
		std::string unique_addr = gen_unique_addr();
		zmq::socket_t pull(ctx, zmq::socket_type::pull);
		pull.bind(unique_addr);

		std::function<void()> on_complete = [&]() {
			zmq::socket_t push(ctx, zmq::socket_type::push);
			push.connect(unique_addr);
			push.send(zmq::buffer(msg::READY));
		};

		// Launch
		std::future<void> result = std::async(std::launch::async, thread, &ctx, on_complete);

		// Wait for response
		std::vector<zmq::message_t> recv_msgs;
		auto res = zmq::recv_multipart(listener, std::back_inserter(recv_msgs));
		assert(res);
		assert(recv_msgs[0].to_string_view() == msg::READY);

		// Done
		return result;
	}
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
