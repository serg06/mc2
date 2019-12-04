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
	std::vector<Quad3D> quads3d;

	inline int size() {
		return quads3d.size();
	}
};

#endif /* __MINICHUNK_MESH__ */

