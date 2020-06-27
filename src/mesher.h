#pragma once

#include "messaging.h"
#include "unique_priority_queue.h"
#include "world_utils.h"

#include "vmath.h"
#include "zmq.hpp"

#include <memory>

void MeshingThread2(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready);

class Mesher
{
public:
	Mesher(std::shared_ptr<zmq::context_t> ctx_);
	~Mesher() = default;
	
	void run(msg::on_ready_fn on_ready);

private:
	bool read_msg(bool wait, std::vector<zmq::message_t>& msg);
	void handle_all_messages(bool wait_for_first, bool& stop);
	void on_msg(const std::vector<zmq::message_t>& msg, bool& stop);
	bool handle_queued_request();
	void on_mesh_gen_request(MeshGenRequest* req);
	void update_player_coords(const vmath::ivec2& new_cords);

private:
	std::shared_ptr<zmq::context_t> ctx;
	BusNode bus;

	// Player's last-known coords (so we always generate meshes closest to here)
	vmath::ivec2 player_coords;

	// Keep queue of incoming requests (based on distance to player)
	using key_t = vmath::ivec3;
	using priority_t = float;
	using pq_T = std::pair<priority_t, MeshGenRequest*>;
	unique_priority_queue<key_t, pq_T, vecN_hash, std::equal_to<key_t>, FirstComparator<priority_t, MeshGenRequest*, std::greater<priority_t>>> request_queue;
	using queue_T = decltype(request_queue);
	using val_T = queue_T::value_type;
};
