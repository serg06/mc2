#ifndef __MINICHUNK_MESH__
#define __MINICHUNK_MESH__

#include "block.h"
#include "render.h"
#include "util.h"

#include <vector>
#include <vmath.h>

//struct Quad {
//	// block face
//	uint8_t face;
//	Block block;
//
//	// x and z are 1/16th of reality
//	uint8_t mini_coords[4];
//};

//struct Quad {
//	// for debugging
//	int size;
//
//	// block face
//	bool is_back_face;
//	Block block;
//
//	// coordinates of corners of quad
//	ivec3 corners[4];
//};

// A mesh of a minichunk, consisting of a bunch of quads & minichunk coordinates
class MiniChunkMesh {
public:
	std::vector<Quad> quads;
	std::vector<Quad3D> quads3d;

	inline int size() {
		return quads.size();
	}
};

inline vmath::ivec4 face_to_direction(int face) {
	switch (face) {
	case 0: return IEAST_0;
	case 1: return IUP_0;
	case 2: return ISOUTH_0;
	case 3: return IWEST_0;
	case 4: return IDOWN_0;
	case 5: return INORTH_0;
	}
}

#endif /* __MINICHUNK_MESH__ */

