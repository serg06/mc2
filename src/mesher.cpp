#include "mesher.h"

#include "world_meshing.h"

#include "zmq_addon.hpp"

#include <cassert>


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
		// If no queued requests, wait for a message to come in
		bool wait_for_first = pq.size() == 0;
		handle_all_messages(wait_for_first, stop);
		if (stop) break;

		// Handle a queued request if exists
		handle_queued_request();
	}
}

void Mesher::handle_all_messages(bool wait_for_first, bool& stop)
{
	std::vector<zmq::message_t> msg;
	bool wait = wait_for_first;
	while (read_msg(wait, msg))
	{
		on_msg(msg, stop);
		wait = false;
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
#ifndef _DEBUG
		WindowsException("unknown message");
#endif // _DEBUG
	}
}

bool Mesher::read_msg(bool wait, std::vector<zmq::message_t>& msg)
{
	msg.clear();
	auto flags = wait ? zmq::recv_flags::none : zmq::recv_flags::dontwait;
	auto ret = zmq::recv_multipart(bus.out, std::back_inserter(msg), flags);
	return ret > 0;
}

bool Mesher::handle_queued_request()
{
	if (pq.size())
	{
		// handle one
		vmath::ivec3 coords = pq.top().coords;
		pq.pop();
		auto search = reqs.find(coords);
		assert(search != reqs.end());
		MeshGenRequest* req_ = search->second;
		reqs.erase(search);

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
	auto search = reqs.find(req->coords);
	if (search != reqs.end())
	{
		search->second = req;
	}
	else
	{
		pq.emplace(priority, req->coords);
		reqs[req->coords] = req;
	}
}

void Mesher::update_player_coords(const vmath::ivec2& new_coords)
{
	if (new_coords != player_coords)
	{
		player_coords = new_coords;

		// Adjust priority queue priorities:
		std::function<void(pq_entry&)> adjust = [&](pq_entry& e) { e.priority = vmath::distance(vmath::ivec2(e.coords[0], e.coords[2]), player_coords); };
		update_pq_priorities(pq, adjust);
	}
}
