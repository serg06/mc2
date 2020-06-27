#pragma once

#include <cassert>
#include <string>
#include <unordered_map>

constexpr int MAX_BLOCK_TYPES = 256;

enum class BlockSolidity {
	Solid,
	Liquid,
	Other
};

class BlockType {
public:
	enum Value : uint8_t
	{
		Air = 0,
		Stone = 1,
		Grass = 2,
		Dirt = 3,
		Cobblestone = 4,
		OakWoodPlank = 5,
		OakSapling = 6,
		Bedrock = 7,
		FlowingWater = 8,
		StillWater = 9,
		FlowingLava = 10,
		StillLava = 11,
		Sand = 12,
		Gravel = 13,
		GoldOre = 14,
		IronOre = 15,
		CoalOre = 16,
		OakWood = 17,
		OakLeaves = 18,
		Sponge = 19,
		Glass = 20,
		LapisLazuliOre = 21,
		LapisLazuliBlock = 22,
		Dispenser = 23,
		Sandstone = 24,
		NoteBlock = 25,
		Bed = 26,
		PoweredRail = 27,
		DetectorRail = 28,
		StickyPiston = 29,
		Cobweb = 30,
		DeadShrub = 31,
		DeadBush = 32,
		Piston = 33,
		PistonHead = 34,
		WhiteWool = 35,
		Dandelion = 37,
		Poppy = 38,
		BrownMushroom = 39,
		RedMushroom = 40,
		GoldBlock = 41,
		IronBlock = 42,
		DoubleStoneSlab = 43,
		StoneSlab = 44,
		Bricks = 45,
		TNT = 46,
		Bookshelf = 47,
		MossStone = 48,
		Obsidian = 49,
		Torch = 50,
		Fire = 51,
		MonsterSpawner = 52,
		OakWoodStairs = 53,
		Chest = 54,
		RedstoneWire = 55,
		DiamondOre = 56,
		DiamondBlock = 57,
		CraftingTable = 58,
		WheatCrops = 59,
		Farmland = 60,
		Furnace = 61,
		BurningFurnace = 62,
		StandingSignBlock = 63,
		OakDoorBlock = 64,
		Ladder = 65,
		Rail = 66,
		CobblestoneStairs = 67,
		WallMountedSignBlock = 68,
		Lever = 69,
		StonePressurePlate = 70,
		IronDoorBlock = 71,
		WoodenPressurePlate = 72,
		RedstoneOre = 73,
		GlowingRedstoneOre = 74,
		RedstoneTorchOff = 75,
		RedstoneTorchOn = 76,
		StoneButton = 77,
		Snow = 78,
		Ice = 79,
		SnowBlock = 80,
		Cactus = 81,
		Clay = 82,
		SugarCanes = 83,
		Jukebox = 84,
		OakFence = 85,
		Pumpkin = 86,
		Netherrack = 87,
		SoulSand = 88,
		Glowstone = 89,
		NetherPortal = 90,
		JackoLantern = 91,
		CakeBlock = 92,
		RedstoneRepeaterBlockOff = 93,
		RedstoneRepeaterBlockOn = 94,
		WhiteStainedGlass = 95,
		WoodenTrapdoor = 96,
		StoneMonsterEgg = 97,
		StoneBricks = 98,
		BrownMushroomBlock = 99,
		RedMushroomBlock = 100,
		IronBars = 101,
		GlassPane = 102,
		MelonBlock = 103,
		PumpkinStem = 104,
		MelonStem = 105,
		Vines = 106,
		OakFenceGate = 107,
		BrickStairs = 108,
		StoneBrickStairs = 109,
		Mycelium = 110,
		LilyPad = 111,
		NetherBrick = 112,
		NetherBrickFence = 113,
		NetherBrickStairs = 114,
		NetherWart = 115,
		EnchantmentTable = 116,
		BrewingStand = 117,
		Cauldron = 118,
		EndPortal = 119,
		EndPortalFrame = 120,
		EndStone = 121,
		DragonEgg = 122,
		RedstoneLampOff = 123,
		RedstoneLampOn = 124,
		DoubleOakWoodSlab = 125,
		OakWoodSlab = 126,
		Cocoa = 127,
		SandstoneStairs = 128,
		EmeraldOre = 129,
		EnderChest = 130,
		TripwireHook = 131,
		Tripwire = 132,
		EmeraldBlock = 133,
		SpruceWoodStairs = 134,
		BirchWoodStairs = 135,
		JungleWoodStairs = 136,
		CommandBlock = 137,
		Beacon = 138,
		CobblestoneWall = 139,
		FlowerPot = 140,
		Carrots = 141,
		Potatoes = 142,
		WoodenButton = 143,
		MobHead = 144,
		Anvil = 145,
		TrappedChest = 146,
		WeightedPressurePlateLight = 147,
		WeightedPressurePlateHeavy = 148,
		RedstoneComparatorOff = 149,
		RedstoneComparatorOn = 150,
		DaylightSensor = 151,
		RedstoneBlock = 152,
		NetherQuartzOre = 153,
		Hopper = 154,
		QuartzBlock = 155,
		QuartzStairs = 156,
		ActivatorRail = 157,
		Dropper = 158,
		WhiteHardenedClay = 159,
		WhiteStainedGlassPane = 160,
		AcaciaLeaves = 161,
		AcaciaWood = 162,
		AcaciaWoodStairs = 163,
		DarkOakWoodStairs = 164,
		SlimeBlock = 165,
		Barrier = 166,
		IronTrapdoor = 167,
		Prismarine = 168,
		SeaLantern = 169,
		HayBale = 170,
		WhiteCarpet = 171,
		HardenedClay = 172,
		BlockofCoal = 173,
		PackedIce = 174,
		Sunflower = 175,
		FreeStandingBanner = 176,
		WallMountedBanner = 177,
		InvertedDaylightSensor = 178,
		RedSandstone = 179,
		RedSandstoneStairs = 180,
		DoubleRedSandstoneSlab = 181,
		RedSandstoneSlab = 182,
		SpruceFenceGate = 183,
		BirchFenceGate = 184,
		JungleFenceGate = 185,
		DarkOakFenceGate = 186,
		AcaciaFenceGate = 187,
		SpruceFence = 188,
		BirchFence = 189,
		JungleFence = 190,
		DarkOakFence = 191,
		AcaciaFence = 192,
		SpruceDoorBlock = 193,
		BirchDoorBlock = 194,
		JungleDoorBlock = 195,
		AcaciaDoorBlock = 196,
		DarkOakDoorBlock = 197,
		EndRod = 198,
		ChorusPlant = 199,
		ChorusFlower = 200,
		PurpurBlock = 201,
		PurpurPillar = 202,
		PurpurStairs = 203,
		PurpurDoubleSlab = 204,
		PurpurSlab = 205,
		EndStoneBricks = 206,
		BeetrootBlock = 207,
		GrassPath = 208,
		EndGateway = 209,
		RepeatingCommandBlock = 210,
		ChainCommandBlock = 211,
		FrostedIce = 212,
		MagmaBlock = 213,
		NetherWartBlock = 214,
		RedNetherBrick = 215,
		BoneBlock = 216,
		StructureVoid = 217,
		Observer = 218,
		WhiteShulkerBox = 219,
		OrangeShulkerBox = 220,
		MagentaShulkerBox = 221,
		LightBlueShulkerBox = 222,
		YellowShulkerBox = 223,
		LimeShulkerBox = 224,
		PinkShulkerBox = 225,
		GrayShulkerBox = 226,
		LightGrayShulkerBox = 227,
		CyanShulkerBox = 228,
		PurpleShulkerBox = 229,
		BlueShulkerBox = 230,
		BrownShulkerBox = 231,
		GreenShulkerBox = 232,
		RedShulkerBox = 233,
		BlackShulkerBox = 234,
		WhiteGlazedTerracotta = 235,
		OrangeGlazedTerracotta = 236,
		MagentaGlazedTerracotta = 237,
		LightBlueGlazedTerracotta = 238,
		YellowGlazedTerracotta = 239,
		LimeGlazedTerracotta = 240,
		PinkGlazedTerracotta = 241,
		GrayGlazedTerracotta = 242,
		LightGrayGlazedTerracotta = 243,
		CyanGlazedTerracotta = 244,
		PurpleGlazedTerracotta = 245,
		BlueGlazedTerracotta = 246,
		BrownGlazedTerracotta = 247,
		GreenGlazedTerracotta = 248,
		RedGlazedTerracotta = 249,
		BlackGlazedTerracotta = 250,
		WhiteConcrete = 251,
		WhiteConcretePowder = 252,
		Outline = 254, // custom
		StructureBlock = 255
	};

	constexpr inline BlockType() : BlockType(Air) {}
	constexpr inline BlockType(uint8_t value) : BlockType((Value)value) {}
	constexpr inline BlockType(Value value) : value(value) {}
	
	// wew
	inline operator Value() const { return value; }
	inline operator uint8_t() const { return value; }
	inline operator int() const { return value; }

	// prevent if(block)
	explicit operator bool() = delete;
	
	// comparing to Block
	constexpr inline bool operator==(BlockType b) const { return value == b.value; }
	constexpr inline bool operator!=(BlockType b) const { return value != b.value; }

	// comparing to Block::Value
	constexpr inline bool operator==(BlockType::Value v) const { return value == v; }
	constexpr inline bool operator!=(BlockType::Value v) const { return value != v; }

	// maps to texture names (unordered maps aren't an ideal solution, but it's the cleanest solution I can think of ATM)
	static const std::unordered_map<Value, std::string> top_texture_names;
	static const std::unordered_map<Value, std::string> side_texture_names;
	static const std::unordered_map<Value, std::string> bottom_texture_names;
	static const bool translucent_blocks[MAX_BLOCK_TYPES];
	static const bool nonsolid_blocks[MAX_BLOCK_TYPES];

	// check if transparent
	constexpr inline bool is_transparent() const {
		return value == BlockType::Air;
	}

	// check if translucent
	constexpr inline bool is_translucent() const {
		return translucent_blocks[value];
	}

	// check if solid
	constexpr inline bool is_solid() const {
		return !nonsolid_blocks[value];
	}

	// check if non-solid
	constexpr inline bool is_nonsolid() const {
		return nonsolid_blocks[value];
	}

	// get from map
	inline std::string top_texture() const {
		auto search = top_texture_names.find(value);

		// if element doesn't exist, null
		if (search == top_texture_names.end()) {
			return {};
		}

		return search->second;
	}

	// get from map
	inline std::string side_texture() const {
		auto search = side_texture_names.find(value);

		// if element doesn't exist, null
		if (search == side_texture_names.end()) {
			return {};
		}

		return search->second;
	}

	// get from map
	inline std::string bottom_texture() const {
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
