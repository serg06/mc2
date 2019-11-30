#include <assert.h>
#include <string>
#include <vmath.h>

enum class Block : uint8_t {
	Air = 0,
	Grass = 1,
	Stone = 2,
};

inline std::string block_name(Block b) {
	switch (b) {
	case Block::Air:
		return "Air";
	case Block::Grass:
		return "Grass";
	case Block::Stone:
		return "Stone";
	default:
		throw "Unknown block type.";
	}
}

inline vmath::vec3 block_color(Block b) {
	switch (b) {
	case Block::Air:
		throw "Air has no color.";
	case Block::Grass:
		return { 34 / 255.0f, 161 / 255.0f, 51 / 255.0f };
	case Block::Stone:
		return { 84 / 255.0f, 89 / 255.0f, 86 / 255.0f };
	default:
		throw "Unknown block type.";
	}
}


