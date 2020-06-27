#include "messaging.h"

#include "zmq_addon.hpp"

#include <future>
#include <sstream>
#include <string>


namespace msg
{
	std::string gen_unique_addr()
	{
		// Not actually that random!
		static std::atomic_uint64_t extra(0);
		std::string suffix = std::to_string(extra.fetch_add(1)); // TODO: better memory order
		std::ostringstream out;
		out << "inproc://unique-" << suffix;
		return out.str();
	}

	std::future<void> launch_thread_wait_until_ready(std::shared_ptr<zmq::context_t> ctx, notifier_thread thread)
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
}

// TODO: Add operator>> and operator<< ? Or maybe add them to zmq.hpp (and submit a PR)?
BusNode::BusNode(std::shared_ptr<zmq::context_t> ctx_) : ctx(ctx_), in(*ctx_, zmq::socket_type::pub), out(*ctx_, zmq::socket_type::sub) {
	in.connect(addr::MSG_BUS_IN);
	out.connect(addr::MSG_BUS_OUT);

#ifdef HUGE_BUS_NODES
	bus_in.setsockopt(ZMQ_SNDHWM, 1000 * 1000);
	bus_out.setsockopt(ZMQ_RCVHWM, 1000 * 1000);
#endif // HUGE_BUS_NODES
}
