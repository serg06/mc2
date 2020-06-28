#include "chunker.h"
#include "game.h"
#include "mesher.h"
#include "messaging.h"

#ifdef _DEBUG
#include "zmq_addon.hpp"
#endif // _DEBUG

#include <memory>

void MessageBus(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready)
{
	// create pub/sub
	zmq::socket_t publisher(*ctx, zmq::socket_type::pub);
	publisher.setsockopt(ZMQ_SNDHWM, 1000 * 1000);
	publisher.bind(addr::MSG_BUS_OUT);

	zmq::socket_t subscriber(*ctx, zmq::socket_type::sub);
	subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	subscriber.setsockopt(ZMQ_RCVHWM, 1000 * 1000);
	subscriber.bind(addr::MSG_BUS_IN);

	// create control socket
	zmq::socket_t control(*ctx, zmq::socket_type::pair);
	control.bind(addr::MSG_BUS_CONTROL);

	// connect to creator and tell them we're ready
	on_ready();

	// receive and send repeatedly
	zmq::proxy_steerable(subscriber, publisher, nullptr, control);
}

#ifdef _DEBUG
void ListenerThread(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready)
{
	// connect to bus
	BusNode bus(ctx);
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);

	// signal ready
	on_ready();

	// print all messages
	bool stop = false;
	while (!stop)
	{
		std::vector<zmq::message_t> message;
		auto ret = zmq::recv_multipart(bus.out, std::back_inserter(message));
		assert(ret);
		std::stringstream out;
		out << "Listener: " << message[0].to_string_view() << "\n";
		OutputDebugString(out.str().c_str());
		if (message[0].to_string_view() == msg::EXIT)
		{
			stop = true;
		}
	}
}

void SenderThread(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready)
{
	// connect to bus
	BusNode bus(ctx);
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);

	// signal ready
	on_ready();

	// send int every second
	int i = 0;
	bool stop = false;
	std::chrono::time_point<std::chrono::high_resolution_clock> last_send_time = (std::numeric_limits<std::chrono::time_point<std::chrono::high_resolution_clock>>::max)();
	while (!stop)
	{
		// read in all messages and check for exit message
		std::vector<zmq::message_t> message_in;
		auto ret_in = zmq::recv_multipart(bus.out, std::back_inserter(message_in), zmq::recv_flags::dontwait);
		while (ret_in)
		{
			if (message_in[0].to_string_view() == msg::EXIT)
			{
				stop = true;
				break;
			}
			message_in.clear();
			ret_in = zmq::recv_multipart(bus.out, std::back_inserter(message_in), zmq::recv_flags::dontwait);
		}

		// if it's time to send, send
		auto now = std::chrono::high_resolution_clock::now();
		if (now - last_send_time > std::chrono::seconds(1))
		{
			std::string tmp = std::to_string(i++);
			std::vector<zmq::const_buffer> message_out({
				zmq::str_buffer("test"),
				zmq::buffer(tmp),
				});
			auto ret_out = zmq::send_multipart(bus.in, message_out, zmq::send_flags::dontwait);
			assert(ret_out);
			last_send_time = now;
		}

		// sleep
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
#endif // _DEBUG


int main()
{
	// Create ZMQ messaging context
	std::shared_ptr<zmq::context_t> ctx = std::make_shared<zmq::context_t>(0);

	// launch message bus
	auto msg_bus_thread = msg::launch_thread_wait_until_ready(ctx, MessageBus);
	zmq::socket_t msg_bus_control(*ctx, zmq::socket_type::pair);
	msg_bus_control.connect(addr::MSG_BUS_CONTROL);

	// launch mesh gen threads
	auto mesh_gen_thread = msg::launch_thread_wait_until_ready(ctx, MeshingThread2);

	// launch chunk gen threads
	auto chunk_gen_thread = msg::launch_thread_wait_until_ready(ctx, ChunkGenThread2);

#ifdef _DEBUG
	// launch listener
	auto listener_thread = msg::launch_thread_wait_until_ready(ctx, ListenerThread);

	// launch sender
	auto sender_thread = msg::launch_thread_wait_until_ready(ctx, SenderThread);
#endif // _DEBUG

	// Run game!
	// TODO: Run on separate thread and join all threads? Or maybe do that inside of run_game()?
	run_game(ctx);

	// Debug
	mesh_gen_thread.wait();
	chunk_gen_thread.wait();

#ifdef _DEBUG
	listener_thread.wait();
	sender_thread.wait();
#endif // _DEBUG

	// Everyone's done, shut down bus
	auto ret = msg_bus_control.send(zmq::buffer(msg::TERMINATE));
	msg_bus_thread.wait();
	assert(ret);
}

#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	main();
}
#endif
