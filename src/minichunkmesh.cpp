#include "minichunkmesh.h"

// A mesh of a minichunk, consisting of a bunch of quads & minichunk coordinates
int MiniChunkMesh::size() const
{
	return quads3d.size();
}

const std::vector<Quad3D>& MiniChunkMesh::get_quads() const
{
	return quads3d;
}

void MiniChunkMesh::add_quad(const Quad3D& quad)
{
	quads3d.push_back(quad);
}
