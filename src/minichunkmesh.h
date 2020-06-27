#pragma once

#include "render.h"

#include "vmath.h"

#include <vector>

// A mesh of a minichunk, consisting of a bunch of quads & minichunk coordinates
class MiniChunkMesh {
public:
	int size() const;
	const std::vector<Quad3D>& get_quads() const;
	void add_quad(const Quad3D& quad);

private:
	std::vector<Quad3D> quads3d;
};
