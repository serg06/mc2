#pragma once

#include "block.h"

#include <array>
#include <utility>

constexpr int INVENTORY_ROWS = 9;
constexpr int INVENTORY_COLS = 4;
constexpr int INVENTORY_SIZE = INVENTORY_ROWS * INVENTORY_COLS;

class Inventory
{
public:
	using Entry = typename std::pair<BlockType, int>;

	Inventory();
	~Inventory() = default;

private:
	std::array<Entry, INVENTORY_SIZE> items;
};
