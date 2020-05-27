#include <future>
#include <iostream>
#include <string>

#include "zmq_addon.hpp"

#define REMOVE

void BusListener(zmq::context_t* ctx, int idx)
{
	// Connect to bus
	zmq::socket_t sock(*ctx, zmq::socket_type::sub);
	sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	sock.connect("inproc://bus-out");

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

		std::stringstream out;
		out << "listener [" << idx << "] recv [" << recv_msgs.size() << "] msgs: [";
		for (int i = 0; i < recv_msgs.size(); i++)
		{
			out << recv_msgs[i].to_string_view();
			if (i != recv_msgs.size() - 1)
			{
				out << ", ";
			}
		}
		out << "]\n";
		OutputDebugString(out.str().c_str());
	}
}

void BusSender(zmq::context_t* ctx, int idx)
{
	// Connect to bus
	zmq::socket_t sock(*ctx, zmq::socket_type::pub);
	sock.connect("inproc://bus-in");

	// Send messages
	while (true)
	{
		std::array<zmq::const_buffer, 2> send_msgs = {
			zmq::str_buffer("foo"),
			zmq::str_buffer("bar!")
		};
		const auto ret = zmq::send_multipart(sock, send_msgs);
		if (!ret)
		{
			OutputDebugString("Sender failed!\n");
			return;
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

void BusProxy(zmq::context_t* ctx)
{
	// create sub/pub
	zmq::socket_t subscriber(*ctx, zmq::socket_type::sub);
	subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	subscriber.bind("inproc://bus-in");

	zmq::socket_t publisher(*ctx, zmq::socket_type::pub);
	publisher.bind("inproc://bus-out");

	// connect to creator and tell them we're ready
	zmq::socket_t pair(*ctx, zmq::socket_type::pair);
	pair.connect("inproc://bus-status");
	pair.send(zmq::str_buffer("done"));

	// disconnect
	pair.close();

	// receive and send repeatedly
	// TODO: Change to proxy_steerable so we can remotely shut it down
	zmq::proxy(subscriber, publisher);

	// wew
	subscriber.close();
	publisher.close();
}

int main()
{
	// Create context
	zmq::context_t ctx(0);

	// Listen to bus start
	zmq::socket_t pair(ctx, zmq::socket_type::pair);
	pair.bind("inproc://bus-status");

	// Start bus
	auto thread1 = std::async(std::launch::async, BusProxy, &ctx);

	// Wait for it to start
	zmq::message_t msg;
	pair.recv(msg);
	assert(msg.to_string_view() == "done");

	// Wew!
	OutputDebugString("Successfully launched bus!\n");

	// Create senders and listeners
	std::vector<std::future<void>> listeners;
	std::vector<std::future<void>> senders;
	for (int i = 0; i < 1; i++)
	{
		listeners.push_back(std::async(std::launch::async, BusListener, &ctx, i));
		senders.push_back(std::async(std::launch::async, BusSender, &ctx, i));
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
