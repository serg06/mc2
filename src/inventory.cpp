#include "inventory.h"

constexpr Inventory::Entry EmptyEntry = Inventory::Entry(BlockType::Air, 0);

Inventory::Inventory()
{
	std::fill(items.begin(), items.end(), EmptyEntry);
}
