#include "game.h"
#include "messaging.h"

#include "GL/gl3w.h"
#include "GL/glcorearb.h"
#include "GLFW/glfw3.h"

#include <Windows.h>

void MessageBus(zmq::context_t* ctx, msg::on_ready_fn on_ready)
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
void ListenerThread(zmq::context_t* ctx, msg::on_ready_fn on_ready)
{
	// connect to bus
	zmq::socket_t bus_in(*ctx, zmq::socket_type::pub);
	bus_in.connect(addr::MSG_BUS_IN);

	zmq::socket_t bus_out(*ctx, zmq::socket_type::sub);
	bus_out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	bus_out.connect(addr::MSG_BUS_OUT);

	// signal ready
	on_ready();

	// print all messages
	bool stop = false;
	while (!stop)
	{
		std::vector<zmq::message_t> message;
		auto ret = zmq::recv_multipart(bus_out, std::back_inserter(message));
		assert(ret);
		std::stringstream out;
		out << "Listener: " << msg::multi_to_str(message) << "\n";
		OutputDebugString(out.str().c_str());
		if (message[0].to_string_view() == msg::EXIT)
		{
			stop = true;
		}
	}
}

void SenderThread(zmq::context_t* ctx, msg::on_ready_fn on_ready)
{
	// connect to bus
	zmq::socket_t bus_in(*ctx, zmq::socket_type::pub);
	bus_in.connect(addr::MSG_BUS_IN);

	zmq::socket_t bus_out(*ctx, zmq::socket_type::sub);
	bus_out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	bus_out.connect(addr::MSG_BUS_OUT);

	// signal ready
	on_ready();

	// send int every second
	int i = 0;
	bool stop = false;
	std::chrono::time_point<std::chrono::high_resolution_clock> last_send_time = std::numeric_limits<std::chrono::time_point<std::chrono::high_resolution_clock>>::max();
	while (!stop)
	{
		// read in all messages and check for exit message
		std::vector<zmq::message_t> message_in;
		auto ret_in = zmq::recv_multipart(bus_out, std::back_inserter(message_in), zmq::recv_flags::dontwait);
		while (ret_in)
		{
			if (message_in[0].to_string_view() == msg::EXIT)
			{
				stop = true;
				break;
			}
			message_in.clear();
			ret_in = zmq::recv_multipart(bus_out, std::back_inserter(message_in), zmq::recv_flags::dontwait);
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
			auto ret_out = zmq::send_multipart(bus_in, message_out, zmq::send_flags::dontwait);
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
	// launch message bus
	auto msg_bus_thread = msg::launch_thread_wait_until_ready(MessageBus);

	// launch mesh gen threads
	auto mesh_gen_thread = msg::launch_thread_wait_until_ready(MeshingThread);

#ifdef _DEBUG
	// launch listener
	auto listener_thread = msg::launch_thread_wait_until_ready(ListenerThread);

	// launch sender
	auto sender_thread = msg::launch_thread_wait_until_ready(SenderThread);
#endif // _DEBUG

	//// test bus
	//zmq::socket_t bus_in(msg::ctx, zmq::socket_type::pub);
	//bus_in.connect(addr::MSG_BUS_IN);
	//while (true)
	//{
	//	auto ret = bus_in.send("test", 4);
	//	assert(ret);
	//	std::this_thread::sleep_for(std::chrono::seconds(1));
	//}

	// Msg bus control socket
	zmq::socket_t msg_bus_control(msg::ctx, zmq::socket_type::pair);
	msg_bus_control.connect(addr::MSG_BUS_CONTROL);

	// Run game!
	run_game();

	// Debug
	mesh_gen_thread.wait();

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
