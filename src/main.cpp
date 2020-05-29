#include "game.h"
#include "messaging.h"

#include "GL/gl3w.h"
#include "GL/glcorearb.h"
#include "GLFW/glfw3.h"

#include <Windows.h>

void MessageBus(zmq::context_t* ctx, msg::on_ready_fn on_ready)
{
	// create sub/pub
	zmq::socket_t subscriber(*ctx, zmq::socket_type::sub);
	subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	subscriber.setsockopt(ZMQ_RCVHWM, 1000 * 1000);
	subscriber.bind(addr::MSG_BUS_IN);

	zmq::socket_t publisher(*ctx, zmq::socket_type::pub);
	publisher.setsockopt(ZMQ_SNDHWM, 1000 * 1000);
	publisher.bind(addr::MSG_BUS_OUT);

	// connect to creator and tell them we're ready
	on_ready();

	// receive and send repeatedly
	// TODO: Change to proxy_steerable so we can remotely shut it down
	zmq::proxy_steerable(subscriber, publisher, nullptr, nullptr);

	// wew
	subscriber.close();
	publisher.close();
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
	while (true)
	{
		std::vector<zmq::message_t> message;
		auto ret = zmq::recv_multipart(bus_out, std::back_inserter(message));
		assert(ret);
		std::stringstream out;
		out << "Listener: " << msg::multi_to_str(message) << "\n";
		OutputDebugString(out.str().c_str());
	}

	bus_in.close();
	bus_out.close();
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
	while (true)
	{
		std::string tmp = std::to_string(i++);
		std::vector<zmq::const_buffer> message({
			zmq::str_buffer("test"),
			zmq::buffer(tmp),
			});
		auto ret = zmq::send_multipart(bus_in, message, zmq::send_flags::dontwait);
		assert(ret);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	bus_in.close();
	bus_out.close();
}
#endif // _DEBUG


int main()
{
	// launch message bus
	auto msg_bus_thread = msg::launch_thread_wait_until_ready(MessageBus);

	// launch mesh gen threads
	auto mesh_gen_thread = msg::launch_thread_wait_until_ready(ChunkGenThread2);

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

	// Run game!
	run_game();
}

#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	main();
}
#endif
