#pragma once

#include "messaging.h"
#include "world_utils.h"

#include "vmath.h"
#include "zmq.hpp"

#include <memory>
#include <queue>
#include <unordered_set>
#include <vector>

void ChunkGenThread2(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready);

struct chunker_pq_entry
{
	chunker_pq_entry(int priority_, const vmath::ivec2& coords_) : priority(priority_), coords(coords_)
	{
	}

	friend bool operator<(const chunker_pq_entry& lhs, const chunker_pq_entry& rhs)
	{
		return lhs.priority < rhs.priority;
	}

	friend bool operator>(const chunker_pq_entry& lhs, const chunker_pq_entry& rhs)
	{
		return lhs.priority > rhs.priority;
	}

	int priority;
	vmath::ivec2 coords;
};

class Chunker
{
public:
	Chunker(std::shared_ptr<zmq::context_t> ctx_);
	~Chunker() = default;

	void run(msg::on_ready_fn on_ready);

private:
	bool read_msg(bool wait, std::vector<zmq::message_t>& msg);
	void handle_all_messages(bool wait_for_first, bool& stop);
	void on_msg(const std::vector<zmq::message_t>& msg, bool& stop);
	bool handle_queued_request();
	void on_chunk_gen_request(std::shared_ptr<ChunkGenRequest> req);
	void update_player_coords(const vmath::ivec2& new_cords);

private:
	std::shared_ptr<zmq::context_t> ctx;
	BusNode bus;

	// Player's last-known coords (so we always generate meshes closest to here)
	vmath::ivec2 player_coords;

	// Keep queue of incoming requests (based on distance to player)
	std::priority_queue<chunker_pq_entry, std::vector<chunker_pq_entry>, std::greater<chunker_pq_entry>> pq;
	std::unordered_set<vmath::ivec2, vecN_hash> reqs;
};
