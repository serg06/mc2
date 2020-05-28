#include <future>
#include <iostream>
#include <string>

#include "messaging.h"
#include "zmq_addon.hpp"

#define REMOVE

// Convert multipart msg to string
std::string multi_to_str(const std::vector<zmq::message_t>& resp)
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

void MeshGenThread(zmq::context_t* ctx, msg::on_ready_fn on_ready)
{
	// Connect to bus
	zmq::socket_t pub(*ctx, zmq::socket_type::pub);
	pub.connect("inproc://bus-in");
	zmq::socket_t sub(*ctx, zmq::socket_type::sub);
	std::vector<std::string> incoming_messages({
		msg::START,
		msg::MESH_GEN_REQUEST
	});
	for (const auto& s : incoming_messages)
	{
		sub.setsockopt(ZMQ_SUBSCRIBE, s.c_str(), s.size());
	}
	//sub.setsockopt(ZMQ_SUBSCRIBE, msg::START.c_str(), 5);
	//sub.setsockopt(ZMQ_SUBSCRIBE, "MESH_GEN_REQUEST", 16);
	sub.connect("inproc://bus-out");

	// Prove you're connected
	on_ready();

	// Wait for START signal before running
	while (true)
	{
		std::vector<zmq::message_t> recv_msgs;
		const auto ret = zmq::recv_multipart(sub, std::back_inserter(recv_msgs));
		if (ret)
		{
			if (recv_msgs[0].to_string_view() == msg::START)
			{
				OutputDebugString("START success!\n");
				break;
			}
		}
		else
		{
			OutputDebugString("START failed\n");
			return;
		}
	}

	// Start running
	while (true)
	{
		std::vector<zmq::message_t> recv_msgs;
		const auto ret = zmq::recv_multipart(sub, std::back_inserter(recv_msgs), zmq::recv_flags::dontwait);
		if (ret)
		{
			std::stringstream s;
			s << "MeshGenThread: " << multi_to_str(recv_msgs) << "\n";
			OutputDebugString(s.str().c_str());
			if (recv_msgs[0].to_string_view() == msg::MESH_GEN_REQUEST)
			{
				std::vector<zmq::const_buffer> send_msgs({
					zmq::buffer(msg::MESH_GEN_RESPONSE),
					zmq::str_buffer("body of response :)")
					});
				zmq::send_multipart(pub, send_msgs);
			}
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
}

void BusListener(zmq::context_t* ctx, msg::on_ready_fn on_ready)
{
	int idx = 0;

	// Connect to bus
	zmq::socket_t sock(*ctx, zmq::socket_type::sub);
	sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	sock.connect("inproc://bus-out");

	// Prove you're connected
	on_ready();

	// Receive messages
	while (true)
	{
		std::vector<zmq::message_t> recv_msgs;
		const auto ret = zmq::recv_multipart(sock, std::back_inserter(recv_msgs));
		if (!ret)
		{
			OutputDebugString("Listener failed!\n");
			return;
		}

		std::stringstream s;
		s << "BusListener: " << multi_to_str(recv_msgs) << "\n";
		OutputDebugString(s.str().c_str());
	}
}

void BusSender(zmq::context_t* ctx, msg::on_ready_fn on_ready)
{
	int idx = 0;

	// Connect to bus
	zmq::socket_t sock(*ctx, zmq::socket_type::pub);
	sock.connect("inproc://bus-in");

	// Prove you're connected
	on_ready();

	// Send messages
	while (true)
	{
		std::array<zmq::const_buffer, 2> send_msgs = {
			zmq::buffer(msg::MESH_GEN_REQUEST),
			zmq::str_buffer("request body LULW")
		};
		const auto ret = zmq::send_multipart(sock, send_msgs);
		if (!ret)
		{
			OutputDebugString("Sender failed!\n");
			return;
		}

		send_msgs = {
			zmq::buffer(msg::START),
			zmq::str_buffer("start bodyyyyy")
		};
		const auto ret2 = zmq::send_multipart(sock, send_msgs);
		if (!ret2)
		{
			OutputDebugString("Sender failed!\n");
			return;
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

namespace
{
	void BusProxy(zmq::context_t* ctx, msg::on_ready_fn on_ready)
	{
		// create sub/pub
		zmq::socket_t subscriber(*ctx, zmq::socket_type::sub);
		subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
		subscriber.bind("inproc://bus-in");

		zmq::socket_t publisher(*ctx, zmq::socket_type::pub);
		publisher.bind("inproc://bus-out");

		// connect to creator and tell them we're ready
		on_ready();

		// receive and send repeatedly
		// TODO: Change to proxy_steerable so we can remotely shut it down
		zmq::proxy_steerable(subscriber, publisher, nullptr, nullptr);

		// wew
		subscriber.close();
		publisher.close();
	}
}

int main()
{
	// Create context
	zmq::context_t ctx(0);

	// Start bus
	auto thread1 = msg::launch_thread_wait_until_ready(ctx, BusProxy);

	// Wew!
	OutputDebugString("Successfully launched bus!\n");

	// Launch all threads one at a time
	std::vector<std::future<void>> meshers;
	for (int i = 0; i < 1; i++)
	{
		meshers.push_back(msg::launch_thread_wait_until_ready(ctx, MeshGenThread));
	}

	// Create senders and listeners
	std::vector<std::future<void>> listeners;
	std::vector<std::future<void>> senders;
	for (int i = 0; i < 1; i++)
	{
		listeners.push_back(msg::launch_thread_wait_until_ready(ctx, BusListener));
		senders.push_back(msg::launch_thread_wait_until_ready(ctx, BusSender));
	}

	OutputDebugString("Launched listeners.\n");

	return 0;
}

#ifdef REMOVE
#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	main();
}
#endif // _WIN32
#endif // REMOVE
