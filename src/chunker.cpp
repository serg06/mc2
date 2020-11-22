#include "chunker.h"

#include "world_meshing.h"

#include "zmq_addon.hpp"

#include <cassert>


using namespace vmath;
using namespace std;


void ChunkGenThread2(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready)
{
	Chunker c(ctx);
	c.run(on_ready);
}

Chunker::Chunker(std::shared_ptr<zmq::context_t> ctx_) : ctx(ctx_), bus(ctx_), player_coords({ 0, 0 })
{
#ifdef _DEBUG
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
#else
	for (const auto& m : msg::chunk_gen_thread_incoming)
	{
		// TODO: Upgrade cppzmq and replace this with .set()
		bus.out.setsockopt(ZMQ_SUBSCRIBE, m.c_str(), m.size());
	}
#endif // _DEBUG
}

// thread for generating new chunk meshes
void Chunker::run(msg::on_ready_fn on_ready) {
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

void Chunker::handle_all_messages(bool wait_for_first, bool& stop)
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

void Chunker::on_msg(const std::vector<zmq::message_t>& msg, bool& stop)
{
	if (msg[0].to_string_view() == msg::EXIT)
	{
		stop = true;
	}
	else if (msg[0].to_string_view() == msg::CHUNK_GEN_REQUEST)
	{
		// Enqueue any chunking requests
		// TODO: Just enqueue, then later distribute to workers.
		ChunkGenRequest* req_ = *(msg[1].data<ChunkGenRequest*>());
		assert(req_);
		std::shared_ptr<ChunkGenRequest> req(req_);
		on_chunk_gen_request(req);
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

bool Chunker::read_msg(bool wait, std::vector<zmq::message_t>& msg)
{
	msg.clear();
	auto flags = wait ? zmq::recv_flags::none : zmq::recv_flags::dontwait;
	auto ret = zmq::recv_multipart(bus.out, std::back_inserter(msg), flags);
	return ret > 0;
}

bool Chunker::handle_queued_request()
{
	if (pq.size())
	{
		// handle one
		vmath::ivec2 coords = pq.top().coords;
		pq.pop();
		auto search = reqs.find(coords);
		assert(search != reqs.end());
		reqs.erase(search);

		// generate a chunk
		ChunkGenResponse* response = new ChunkGenResponse;
		response->coords = coords;
		response->chunk = std::make_unique<Chunk>(coords);
		response->chunk->generate();

		// send it
		std::vector<zmq::const_buffer> result({
			zmq::buffer(msg::CHUNK_GEN_RESPONSE),
			zmq::buffer(&response, sizeof(response))
			});
		auto ret = zmq::send_multipart(bus.in, result, zmq::send_flags::dontwait);
		assert(ret);

		return true;
	}

	return false;
}

void Chunker::on_chunk_gen_request(std::shared_ptr<ChunkGenRequest> req)
{
	if (!reqs.contains(req->coords))
	{
		float priority = vmath::distance(req->coords, player_coords);
		pq.emplace(static_cast<int>(priority), req->coords);
		reqs.insert(req->coords);
	}
}

void Chunker::update_player_coords(const vmath::ivec2& new_coords)
{
	if (new_coords != player_coords)
	{
		player_coords = new_coords;

		// Adjust priority queue priorities:
		std::function<void(chunker_pq_entry&)> adjust = [&](chunker_pq_entry& e) { e.priority = vmath::distance(vmath::ivec2(e.coords[0], e.coords[2]), player_coords); };
		update_pq_priorities(pq, adjust);
	}
}
