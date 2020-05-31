#include <future>
#include <iostream>
#include <string>

#include "messaging.h"

#include "zmq_addon.hpp"

#include <Windows.h>

// TODO: Add operator>> and operator<< ? Or maybe add them to zmq.hpp (and submit a PR)?
BusNode::BusNode(zmq::context_t* const ctx_) : ctx(ctx_), in(*ctx_, zmq::socket_type::pub), out(*ctx_, zmq::socket_type::sub) {
	in.connect(addr::MSG_BUS_IN);
	out.connect(addr::MSG_BUS_OUT);

#ifdef HUGE_BUS_NODES
	bus_in.setsockopt(ZMQ_SNDHWM, 1000 * 1000);
	bus_out.setsockopt(ZMQ_RCVHWM, 1000 * 1000);
#endif // HUGE_BUS_NODES
}
