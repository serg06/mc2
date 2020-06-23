#pragma once

#include "inventory.h"

#include "vmath.h"

// Actions a player can be performing
struct Actions
{
	bool forwards = false;
	bool backwards = false;
	bool right = false;
	bool left = false;
	bool jumping = false;
	bool shifting = false;
	bool mining = false;
};

class Player
{
public:
	Player() = default;
	~Player() = default;

	// directions relative to the player
	vmath::vec4 staring_direction();
	vmath::vec4 up_direction();
	vmath::vec4 right_direction();

public:
	// World only
	vmath::vec4 velocity = { 0.0f };

	// Both.
	Actions actions;

	// Both. Update in world then reflect in renderer
	vmath::vec4 coords = { 8.0f, 73.0f, 8.0f, 1.0f };
	vmath::ivec2 chunk_coords = { std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
	vmath::ivec3 staring_at = { 0, -1, 0 }; // the block you're staring at (invalid by default)
	vmath::ivec3 staring_at_face; // the face you're staring at on the block you're staring at

	Inventory inventory;

	bool in_water = false;
	bool noclip = false;

	// Both. Update in renderer then reflect in world
	float pitch = 0;
	float yaw = 0;
	BlockType held_block = BlockType::StillWater; // TODO: Instead, remembering which inventory slot

	// TODO: Remove?
	int render_distance = 1;
	bool should_check_for_nearby_chunks = true;
};
