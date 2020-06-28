#pragma once

#include "messaging.h"
#include "unique_priority_queue.h"
#include "world_utils.h"

#include "vmath.h"
#include "zmq.hpp"

#include <memory>

void MeshingThread2(std::shared_ptr<zmq::context_t> ctx, msg::on_ready_fn on_ready);

struct pq_entry
{
	pq_entry(int priority_, const vmath::ivec3& coords_) : priority(priority_), coords(coords_)
	{
	}

	friend bool operator<(const pq_entry& lhs, const pq_entry& rhs)
	{
		return lhs.priority < rhs.priority;
	}

	friend bool operator>(const pq_entry& lhs, const pq_entry& rhs)
	{
		return lhs.priority > rhs.priority;
	}

	int priority;
	vmath::ivec3 coords;
};

template<
	class T,
	class Container = std::vector<T>,
	class Compare = std::less<typename Container::value_type>
>
class priority_queue_hacker : public std::priority_queue<T, Container, Compare>
{
public:
	Container& get_c()
	{
		return c;
	}

	Compare& get_comp()
	{
		return comp;
	}
};

template<class T, class Container, class Compare>
void update_pq_priorities(std::priority_queue<T, Container, Compare>& pq, std::function<void(T&)>& update)
{
	// Get container
	priority_queue_hacker<T, Container, Compare> hacker;
	hacker.swap(pq);
	Container& c = hacker.get_c();

	// Update priorities
	for (auto& entry : c)
	{
		update(entry);
	}

	// Fix heap
	std::make_heap(c.begin(), c.end(), hacker.get_comp());

	// Stick it back in
	pq.swap(hacker);
}

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
	std::priority_queue<pq_entry, std::vector<pq_entry>, std::greater<pq_entry>> pq;
	std::unordered_map<vmath::ivec3, MeshGenRequest*, vecN_hash> reqs;
};
