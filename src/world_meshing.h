#pragma once

#include "minichunkmesh.h"
#include "world_utils.h"

#include <memory>

MeshGenResult* gen_minichunk_mesh_from_req(std::shared_ptr<MeshGenRequest> req);
std::unique_ptr<MiniChunkMesh> gen_minichunk_mesh(std::shared_ptr<MeshGenRequest> req);
