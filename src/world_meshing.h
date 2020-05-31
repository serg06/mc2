#pragma once

#include "world_utils.h"

#include "chunk.h"

#include "vmath.h"
#include "zmq.hpp"

#include <queue>
#include <unordered_map>
#include <unordered_set>

// Rendering part
#include "minichunkmesh.h"
#include "render.h"

using namespace std;

MeshGenResult* gen_minichunk_mesh_from_req(std::shared_ptr<MeshGenRequest> req);
std::unique_ptr<MiniChunkMesh> gen_minichunk_mesh(std::shared_ptr<MeshGenRequest> req);
