#include "block.h"

// set up Block{} static variables
const std::unordered_map<Block::Value, std::string> Block::top_texture_names = {
	{ Block::Stone, "stone" },
	{ Block::Grass, "grass_top" },
	{ Block::StillWater, "water_square" },
	{ Block::OakWood, "tree_top" },
	{ Block::OakLeaves, "leaves" },
	{ Block::DiamondBlock, "blockDiamond" },
	{ Block::Outline, "outline" },
};

const std::unordered_map<Block::Value, std::string> Block::bottom_texture_names = {
	{ Block::Stone, "stone" },
	{ Block::Grass, "dirt" },
	{ Block::StillWater, "water_square" },
	{ Block::OakWood, "tree_top" },
	{ Block::OakLeaves, "leaves" },
	{ Block::DiamondBlock, "blockDiamond" },
	{ Block::Outline, "outline" },
};

const std::unordered_map<Block::Value, std::string> Block::side_texture_names = {
	{ Block::Stone, "stone" },
	{ Block::Grass, "grass_side" },
	{ Block::StillWater, "water_square" },
	{ Block::OakWood, "tree_side" },
	{ Block::OakLeaves, "leaves" },
	{ Block::DiamondBlock, "blockDiamond" },
	{ Block::Outline, "outline" },
};

// 16 x 16 array to quickly look-up if a block is transparent -- I've gotta improve this somehow lmao
const bool Block::translucent_blocks[MAX_BLOCK_TYPES] = {
	true,  false, false, false, false, false, false, false, false, true,  false, false, false, false, false, false,
	false, false, true,  false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
};
