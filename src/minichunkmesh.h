#pragma once

#include "block.h"
#include "render.h"
#include "util.h"
#include "vmath.h"

#include <vector>

// A mesh of a minichunk, consisting of a bunch of quads & minichunk coordinates
class MiniChunkMesh {
private:
	std::vector<Quad3D> quads3d;

public:
	inline int size() const {
		return quads3d.size();
	}

	const inline std::vector<Quad3D>& get_quads() const {
		return quads3d;
	}

	inline void add_quad(const Quad3D& quad) {
		quads3d.push_back(quad);
	}
};
