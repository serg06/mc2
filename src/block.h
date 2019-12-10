#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <assert.h>
#include <string>
#include <unordered_map>
#include <vmath.h>

#define MAX_BLOCK_TYPES 256

enum class BlockSolidity {
	Solid,
	Liquid,
	Other
};

class Block {
public:
	enum Value : uint8_t
	{
		Air = 0,
		Stone = 1,
		Grass = 2,
		Water = 9,
		OakLog = 17,
		OakLeaves = 18,
		Outline = 100,
	};

	constexpr Block() : Block(Air) {}
	constexpr Block(uint8_t value) : Block((Value)value) {}
	constexpr Block(Value value) : value(value) {}
	
	// wew
	operator Value() const { return value; }
	operator uint8_t() const { return value; }

	// prevent if(block)
	explicit operator bool() = delete;
	
	// comparing to Block
	constexpr inline bool operator==(Block b) const { return value == b.value; }
	constexpr inline bool operator!=(Block b) const { return value != b.value; }

	// comparing to Block::Value
	constexpr inline bool operator==(Block::Value v) const { return value == v; }
	constexpr inline bool operator!=(Block::Value v) const { return value != v; }

	// maps to texture names (unordered maps aren't an ideal solution, but it's the cleanest solution I can think of ATM)
	static const std::unordered_map<Value, std::string> top_texture_names;
	static const std::unordered_map<Value, std::string> side_texture_names;
	static const std::unordered_map<Value, std::string> bottom_texture_names;

	// get from map
	std::string top_texture() {
		auto search = top_texture_names.find(value);

		// if element doesn't exist, null
		if (search == top_texture_names.end()) {
			return {};
		}

		return search->second;
	}

	// get from map
	std::string side_texture() {
		auto search = side_texture_names.find(value);

		// if element doesn't exist, null
		if (search == side_texture_names.end()) {
			return {};
		}

		return search->second;
	}

	// get from map
	std::string bottom_texture() {
		auto search = bottom_texture_names.find(value);

		// if element doesn't exist, null
		if (search == bottom_texture_names.end()) {
			return {};
		}

		return search->second;
	}

private:
	Value value;
};

inline std::string block_name(Block b) {
	switch ((Block::Value)b) {
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
	switch ((Block::Value)b) {
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



#endif // __BLOCK_H__
