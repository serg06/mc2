#include "block.h"

// set up Block{} static variables
const std::unordered_map<BlockType::Value, std::string> BlockType::top_texture_names = {
	{ BlockType::Stone, "stone" },
	{ BlockType::Grass, "grass_top" },
	{ BlockType::StillWater, "water_square" },
	{ BlockType::FlowingWater, "water_square_light" },
	{ BlockType::OakWood, "tree_top" },
	{ BlockType::OakLeaves, "leaves" },
	{ BlockType::DiamondBlock, "blockDiamond" },
	{ BlockType::Outline, "outline" },
};

const std::unordered_map<BlockType::Value, std::string> BlockType::bottom_texture_names = {
	{ BlockType::Stone, "stone" },
	{ BlockType::Grass, "dirt" },
	{ BlockType::StillWater, "water_square" },
	{ BlockType::FlowingWater, "water_square_light" },
	{ BlockType::OakWood, "tree_top" },
	{ BlockType::OakLeaves, "leaves" },
	{ BlockType::DiamondBlock, "blockDiamond" },
	{ BlockType::Outline, "outline" },
};

const std::unordered_map<BlockType::Value, std::string> BlockType::side_texture_names = {
	{ BlockType::Stone, "stone" },
	{ BlockType::Grass, "grass_side" },
	{ BlockType::StillWater, "water_square" },
	{ BlockType::FlowingWater, "water_square_light" },
	{ BlockType::OakWood, "tree_side" },
	{ BlockType::OakLeaves, "leaves" },
	{ BlockType::DiamondBlock, "blockDiamond" },
	{ BlockType::Outline, "outline" },
};

// 16 x 16 array to quickly look-up if a block is transparent -- I've gotta improve this somehow lmao
const bool BlockType::translucent_blocks[MAX_BLOCK_TYPES] = {
	// air, flowing water, still water
	true,  false, false, false, false, false, false, false, true, true,  false, false, false, false, false, false,
	// leaves
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

// 16 x 16 array to quickly look-up if a block is transparent -- I've gotta improve this somehow lmao
const bool BlockType::nonsolid_blocks[MAX_BLOCK_TYPES] = {
	// air, flowing water, still water
	true,  false, false, false, false, false, false, false, true, true,  false, false, false, false, false, false,
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
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
};
