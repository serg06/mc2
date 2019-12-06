#ifndef __MINICHUNK_MESH__
#define __MINICHUNK_MESH__

#include "block.h"
#include "render.h"
#include "util.h"

#include <vector>
#include <vmath.h>

// A mesh of a minichunk, consisting of a bunch of quads & minichunk coordinates
class MiniChunkMesh {
public:
	std::vector<Quad3D> quads3d;

	inline int size() {
		return quads3d.size();
	}
};

#endif /* __MINICHUNK_MESH__ */

