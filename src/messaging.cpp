#include <future>
#include <iostream>
#include <string>

#include "messaging.h"

#include "zmq_addon.hpp"

#include <Windows.h>

namespace msg
{
	zmq::context_t ctx(0);

	//std::future<void> run_message_bus(zmq::context_t& ctx)
	//{
	//	return launch_thread_wait_until_ready(ctx, BusProxy);
	//}
}

//
//void MeshGenThread(zmq::context_t* ctx)
//{
//	// Connect to bus
//	zmq::socket_t pub(*ctx, zmq::socket_type::pub);
//	pub.connect("inproc://bus-in");
//	zmq::socket_t sub(*ctx, zmq::socket_type::sub);
//	std::vector<std::string> incoming_messages({
//		msg::START,
//		msg::MESH_GEN_REQUEST
//		});
//	for (const auto& s : incoming_messages)
//	{
//		sub.setsockopt(ZMQ_SUBSCRIBE, s.c_str(), s.size());
//	}
//	//sub.setsockopt(ZMQ_SUBSCRIBE, msg::START.c_str(), 5);
//	//sub.setsockopt(ZMQ_SUBSCRIBE, "MESH_GEN_REQUEST", 16);
//	sub.connect("inproc://bus-out");
//
//	// Prove you're connected
//	zmq::socket_t push(*ctx, zmq::socket_type::push);
//	push.connect("inproc://bus-status");
//	push.send(zmq::buffer(msg::CONNECTED_TO_BUS));
//
//	// Wait for START signal before running
//	while (true)
//	{
//		std::vector<zmq::message_t> recv_msgs;
//		const auto ret = zmq::recv_multipart(sub, std::back_inserter(recv_msgs));
//		if (ret)
//		{
//			if (recv_msgs[0].to_string_view() == msg::START)
//			{
//				OutputDebugString("START success!\n");
//				break;
//			}
//		}
//		else
//		{
//			OutputDebugString("START failed\n");
//			return;
//		}
//	}
//
//	// Start running
//	while (true)
//	{
//		std::vector<zmq::message_t> recv_msgs;
//		const auto ret = zmq::recv_multipart(sub, std::back_inserter(recv_msgs), zmq::recv_flags::dontwait);
//		if (ret)
//		{
//			std::stringstream s;
//			s << "MeshGenThread: " << multi_to_str(recv_msgs) << "\n";
//			OutputDebugString(s.str().c_str());
//			if (recv_msgs[0].to_string_view() == msg::MESH_GEN_REQUEST)
//			{
//				std::vector<zmq::const_buffer> send_msgs({
//					zmq::buffer(msg::MESH_GEN_RESPONSE),
//					zmq::str_buffer("body of response :)")
//					});
//				zmq::send_multipart(pub, send_msgs);
//			}
//		}
//		else
//		{
//			std::this_thread::sleep_for(std::chrono::microseconds(1));
//		}
//	}
//}
//
//void BusListener(zmq::context_t* ctx)
//{
//	int idx = 0;
//
//	// Connect to bus
//	zmq::socket_t sock(*ctx, zmq::socket_type::sub);
//	sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);
//	sock.connect("inproc://bus-out");
//
//	// Prove you're connected
//	zmq::socket_t push(*ctx, zmq::socket_type::push);
//	push.connect("inproc://bus-status");
//	push.send(zmq::buffer(msg::CONNECTED_TO_BUS));
//
//	// Receive messages
//	while (true)
//	{
//		std::vector<zmq::message_t> recv_msgs;
//		const auto ret = zmq::recv_multipart(sock, std::back_inserter(recv_msgs));
//		if (!ret)
//		{
//			OutputDebugString("Listener failed!\n");
//			return;
//		}
//
//		std::stringstream s;
//		s << "BusListener: " << multi_to_str(recv_msgs) << "\n";
//		OutputDebugString(s.str().c_str());
//	}
//}
//
//void BusSender(zmq::context_t* ctx)
//{
//	int idx = 0;
//
//	// Connect to bus
//	zmq::socket_t sock(*ctx, zmq::socket_type::pub);
//	sock.connect("inproc://bus-in");
//
//	// Prove you're connected
//	zmq::socket_t push(*ctx, zmq::socket_type::push);
//	push.connect("inproc://bus-status");
//	push.send(zmq::buffer(msg::CONNECTED_TO_BUS));
//
//	// Send messages
//	while (true)
//	{
//		std::array<zmq::const_buffer, 2> send_msgs = {
//			zmq::buffer(msg::MESH_GEN_REQUEST),
//			zmq::str_buffer("request body LULW")
//		};
//		const auto ret = zmq::send_multipart(sock, send_msgs);
//		if (!ret)
//		{
//			OutputDebugString("Sender failed!\n");
//			return;
//		}
//
//		send_msgs = {
//			zmq::buffer(msg::START),
//			zmq::str_buffer("start bodyyyyy")
//		};
//		const auto ret2 = zmq::send_multipart(sock, send_msgs);
//		if (!ret2)
//		{
//			OutputDebugString("Sender failed!\n");
//			return;
//		}
//
//		std::this_thread::sleep_for(std::chrono::microseconds(1));
//	}
//}
//
//
//
//// message bus
//void BusProxy(zmq::context_t* ctx)
//{
//	// create sub/pub
//	zmq::socket_t subscriber(*ctx, zmq::socket_type::sub);
//	subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
//	subscriber.bind("inproc://bus-in");
//
//	zmq::socket_t publisher(*ctx, zmq::socket_type::pub);
//	publisher.bind("inproc://bus-out");
//
//	// connect to creator and tell them we're ready
//	zmq::socket_t push(*ctx, zmq::socket_type::push);
//	push.connect("inproc://bus-status");
//	push.send(zmq::buffer(msg::BUS_CREATED));
//
//	// disconnect
//	push.close();
//
//	// receive and send repeatedly
//	// TODO: Change to proxy_steerable so we can remotely shut it down
//	zmq::proxy_steerable(subscriber, publisher, nullptr, nullptr);
//
//	// wew
//	subscriber.close();
//	publisher.close();
//}
//
//std::future<void> launch_thread_wait_until_ready(zmq::context_t& ctx, zmq::socket_t& listener, std::function<void(zmq::context_t*)> thread)
//{
//	// Launch
//	std::future<void> result = std::async(std::launch::async, thread, &ctx);
//
//	// Wait for response
//	std::vector<zmq::message_t> recv_msgs;
//	auto res = zmq::recv_multipart(listener, std::back_inserter(recv_msgs));
//	assert(res);
//
//	// Just in case
//	assert(recv_msgs[0].to_string_view() == msg::CONNECTED_TO_BUS);
//
//	return result;
//}
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
