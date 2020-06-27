#include "mesher.h"

#include "chunk.h"
#include "chunkdata.h"
#include "messaging.h"
#include "render.h"
#include "shapes.h"
#include "unique_priority_queue.h"
#include "unique_queue.h"
#include "util.h"
#include "world_meshing.h"
#include "world_render.h"

#include "examples/imgui_impl_opengl3.h"
#include "examples/imgui_impl_glfw.h"
#include "imgui.h"
#include "zmq_addon.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <numeric>
#include <string>

using namespace vmath;
using namespace std;


void MeshingThread2(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready)
{
	Mesher m(ctx);
	m.run(on_ready);
}

Mesher::Mesher(std::shared_ptr<zmq::context_t> ctx_) : ctx(ctx_), bus(ctx_), player_coords({ 0, 0 })
{
#ifdef _DEBUG
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
#else
	for (const auto& m : msg::meshing_thread_incoming)
	{
		// TODO: Upgrade cppzmq and replace this with .set()
		bus.out.setsockopt(ZMQ_SUBSCRIBE, m.c_str(), m.size());
	}
#endif // _DEBUG
}

// thread for generating new chunk meshes
void Mesher::run(msg::on_ready_fn on_ready) {
	// Prove you're connected
	on_ready();

	// Run thread until stopped
	bool stop = false;
	while (!stop)
	{
		handle_all_messages(stop);
		if (stop) break;

		// Handle a queued request
		bool handled = handle_queued_request();

		// None ready, sleep
		if (!handled)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
}

void Mesher::handle_all_messages(bool& stop)
{
	// read multipart message
	std::vector<zmq::message_t> msg;
	while (read_msg(msg))
	{
		on_msg(msg, stop);
		if (stop) break;
	}
}

void Mesher::on_msg(const std::vector<zmq::message_t>& msg, bool& stop)
{
	if (msg[0].to_string_view() == msg::EXIT)
	{
		stop = true;
	}
	else if (msg[0].to_string_view() == msg::MESH_GEN_REQUEST)
	{
		// Enqueue any meshing requests
		// TODO: Just enqueue coords, then later deal them to workers

		MeshGenRequest* req = *(msg[1].data<MeshGenRequest*>());
		assert(req);
		on_mesh_gen_request(req);
	}
	else if (msg[0].to_string_view() == msg::EVENT_PLAYER_MOVED_CHUNKS)
	{
		vmath::ivec2 new_coords = *(msg[1].data<vmath::ivec2>());
		update_player_coords(new_coords);
	}
	else
	{
		assert(false && "unknown message");
	}
}

bool Mesher::read_msg(std::vector<zmq::message_t>& msg)
{
	msg.clear();
	auto ret = zmq::recv_multipart(bus.out, std::back_inserter(msg), zmq::recv_flags::dontwait);
	return ret > 0;
}

bool Mesher::handle_queued_request()
{
	if (request_queue.size())
	{
		// handle one
		MeshGenRequest* req_ = request_queue.top().second.second;
		assert(req_);
		request_queue.pop();

		std::shared_ptr<MeshGenRequest> req(req_);

		// generate a mesh if possible
		MeshGenResult* mesh = gen_minichunk_mesh_from_req(req);
		if (mesh != nullptr)
		{
			// send it
			std::vector<zmq::const_buffer> result({
				zmq::buffer(msg::MESH_GEN_RESPONSE),
				zmq::buffer(&mesh, sizeof(mesh))
				});
			auto ret = zmq::send_multipart(bus.in, result, zmq::send_flags::dontwait);
			assert(ret);
		}

		return true;
	}

	return false;
}

void Mesher::on_mesh_gen_request(MeshGenRequest* req)
{
	vmath::ivec2 chunk_coords = { req->coords[0], req->coords[2] };
	float priority = vmath::distance(chunk_coords, player_coords);
	val_T v = { req->coords, { priority, req } };
	request_queue.push(v);
}

void Mesher::update_player_coords(const vmath::ivec2& new_coords)
{
	if (new_coords != player_coords)
	{
		player_coords = new_coords;

		// Adjust priority queue priorities
		// TODO: Speed this up by making a vector of values and passing that to priority_queue's constructor
		decltype(request_queue) pq2;
		for (const val_T& v : request_queue)
		{
			std::remove_cv_t<std::remove_reference_t<decltype(v)>> v2 = v;
			auto& coords = v2.first;
			vmath::ivec2 chunk_coords = { coords[0], coords[2] };
			auto& priority = v2.second.first;
			priority = vmath::distance(chunk_coords, player_coords);
			pq2.push(v2);
		}
		request_queue.swap(pq2);
	}
}
