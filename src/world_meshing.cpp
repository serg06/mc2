#include "world_meshing.h"

constexpr void gen_working_indices(const int& layers_idx, int& working_idx_1, int& working_idx_2) {
	switch (layers_idx) {
	case 0:
		working_idx_1 = 2; // z
		working_idx_2 = 1; // y
		break;
	case 1:
		working_idx_1 = 0; // x
		working_idx_2 = 2; // z
		break;
	case 2:
		working_idx_1 = 0; // x
		working_idx_2 = 1; // y
		break;
	}
	return;
}

WorldMeshPart::WorldMeshPart(zmq::context_t* const ctx_) : WorldCommonPart(ctx_) {}

bool WorldMeshPart::check_if_covered(std::shared_ptr<MeshGenRequest> req) {
	// if contains any translucent blocks, don't know how to handle that yet
	// TODO?
	if (req->data->self->any_translucent()) {
		return false;
	}

	// none are air, so only check outside blocks
	for (int miniY = 0; miniY < MINICHUNK_HEIGHT; miniY++) {
		for (int miniZ = 0; miniZ < CHUNK_DEPTH; miniZ++) {
			for (int miniX = 0; miniX < CHUNK_WIDTH; miniX++) {
				const auto& mini_coords = req->data->self->get_coords();
				const vmath::ivec3 coords = { mini_coords[0] * CHUNK_WIDTH + miniX, mini_coords[1] + miniY,  mini_coords[2] * CHUNK_DEPTH + miniZ };

				// if along east wall, check east
				if (miniX == CHUNK_WIDTH - 1) {
					if (req->data->east && req->data->east->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
				// if along west wall, check west
				if (miniX == 0) {
					if (req->data->west && req->data->west->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}

				// if along north wall, check north
				if (miniZ == 0) {
					if (req->data->north && req->data->north->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
				// if along south wall, check south
				if (miniZ == CHUNK_DEPTH - 1) {
					if (req->data->south && req->data->south->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}

				// if along bottom wall, check bottom
				if (miniY == 0) {
					if (req->data->down && req->data->down->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
				// if along top wall, check top
				if (miniY == MINICHUNK_HEIGHT - 1) {
					if (req->data->up && req->data->up->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
			}
		}
	}

	return true;
}

// convert 2D quads to 3D quads
// face: for offset
vector<Quad3D> WorldMeshPart::quads_2d_3d(const vector<Quad2D>& quads2d, const int layers_idx, const int layer_no, const vmath::ivec3& face) {
	vector<Quad3D> result(quads2d.size());

	// working variable

	// most efficient to traverse working_idx_1 then working_idx_2;
	int working_idx_1, working_idx_2;
	gen_working_indices(layers_idx, working_idx_1, working_idx_2);

	// for each quad
	for (int i = 0; i < quads2d.size(); i++) {
		auto& quad3d = result[i];
		auto& quad2d = quads2d[i];

		// set block
		quad3d.block = (uint8_t)quad2d.block;

		// convert both corners to 3D coordinates
		quad3d.corner1[layers_idx] = layer_no;
		quad3d.corner1[working_idx_1] = quad2d.corners[0][0];
		quad3d.corner1[working_idx_2] = quad2d.corners[0][1];

		quad3d.corner2[layers_idx] = layer_no;
		quad3d.corner2[working_idx_1] = quad2d.corners[1][0];
		quad3d.corner2[working_idx_2] = quad2d.corners[1][1];

		// set face
		quad3d.face = face;

		// set metadata
		quad3d.metadata = quad2d.metadata;
	}

	return result;
}

// generate layer by grabbing face blocks directly from the minichunk
void WorldMeshPart::gen_layer_generalized(const std::shared_ptr<MiniChunk> mini, const std::shared_ptr<MiniChunk> face_mini, const int layers_idx, const int layer_no, const vmath::ivec3 face, BlockType(&result)[16][16]) {
	// most efficient to traverse working_idx_1 then working_idx_2;
	int working_idx_1, working_idx_2;
	gen_working_indices(layers_idx, working_idx_1, working_idx_2);

	// coordinates of current block
	vmath::ivec3 coords = { 0, 0, 0 };
	coords[layers_idx] = layer_no;

	// reset all to air
	memset(result, (uint8_t)BlockType::Air, sizeof(result));
	if (!mini)
	{
		OutputDebugString("Uh oh 3501\n");
		return;
	}

	// for each coordinate
	for (int v = 0; v < 16; v++) {
		for (int u = 0; u < 16; u++) {
			coords[working_idx_1] = u;
			coords[working_idx_2] = v;

			// get block at these coordinates
			const BlockType block = mini->get_block(coords);

			// skip air blocks
			if (block == BlockType::Air) {
				continue;
			}

			// no face mini => face is air
			else if (face_mini == nullptr) {
				result[u][v] = block;
			}

			// face mini exists
			else {
				// get face block
				vmath::ivec3 face_coords = coords + face;
				face_coords[layers_idx] = (face_coords[layers_idx] + 16) % 16;
				const BlockType face_block = face_mini->get_block(face_coords);

				// if block's face is visible, set it
				if (is_face_visible(block, face_block)) {
					result[u][v] = block;
				}
			}
		}
	}
}

bool WorldMeshPart::is_face_visible(const BlockType& block, const BlockType& face_block) {
	return face_block.is_transparent() || (block != BlockType::StillWater && block != BlockType::FlowingWater && face_block.is_translucent()) || (face_block.is_translucent() && !block.is_translucent());
}

void WorldMeshPart::gen_layer(const std::shared_ptr<MeshGenRequest> req, const int layers_idx, const int layer_no, const vmath::ivec3& face, BlockType(&result)[16][16]) {
	// get coordinates of a random block
	vmath::ivec3 coords = { 0, 0, 0 };
	coords[layers_idx] = layer_no;
	const vmath::ivec3 face_coords = coords + face;

	// figure out which mini has our face layer (usually ours)
	std::shared_ptr<MiniChunk> mini = req->data->self;
	std::shared_ptr<MiniChunk> face_mini = nullptr;
	if (in_range(face_coords, vmath::ivec3(0, 0, 0), vmath::ivec3(15, 15, 15))) {
		face_mini = mini;
	}
	else {
		const auto face_mini_coords = mini->get_coords() + (layers_idx == 1 ? face * 16 : face);

#define SET_COORDS(ATTR)\
			if (face_mini == nullptr && req->data->ATTR && req->data->ATTR->get_coords() == face_mini_coords)\
			{\
				face_mini = req->data->ATTR;\
			}

		SET_COORDS(up);
		SET_COORDS(down);
		SET_COORDS(north);
		SET_COORDS(south);
		SET_COORDS(east);
		SET_COORDS(west);
#undef SET_COORDS
	}

	// generate layer
	gen_layer_generalized(mini, face_mini, layers_idx, layer_no, face, result);
}

// given 2D array of block numbers, generate optimal quads
vector<Quad2D> WorldMeshPart::gen_quads(const BlockType(&layer)[16][16], /* const Metadata(&metadata_layer)[16][16], */ bool(&merged)[16][16]) {
	memset(merged, false, sizeof(merged));

	vector<Quad2D> result;

	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 16; j++) {
			// skip merged blocks
			if (merged[i][j]) continue;

			const BlockType block = layer[i][j];

			// skip air
			if (block == BlockType::Air) continue;

			// get max size of this quad
			const vmath::ivec2 max_size = get_max_size(layer, merged, { i, j }, block);

			// add it to results
			const vmath::ivec2 start = { i, j };
			Quad2D q;
			q.block = block;
			q.corners[0] = start;
			q.corners[1] = start + max_size;
			//if (block == BlockType::FlowingWater) {
			//	q.metadata = metadata_layer[i][j];
			//}

			// mark all as merged
			mark_as_merged(merged, start, max_size);

			// wew
			result.push_back(q);
		}
	}

	return result;
}

void WorldMeshPart::mark_as_merged(bool(&merged)[16][16], const vmath::ivec2& start, const vmath::ivec2& max_size) {
	for (int i = start[0]; i < start[0] + max_size[0]; i++) {
		for (int j = start[1]; j < start[1] + max_size[1]; j++) {
			merged[i][j] = true;
		}
	}
}

// given a layer and start point, find its best dimensions
vmath::ivec2 WorldMeshPart::get_max_size(const BlockType(&layer)[16][16], const bool(&merged)[16][16], const vmath::ivec2& start_point, const BlockType& block_type) {
	assert(block_type != BlockType::Air);
	assert(!merged[start_point[0]][start_point[1]] && "bruh");

	// TODO: Search width with find() instead of a for loop?

	// max width and height
	vmath::ivec2 max_size = { 1, 1 };

	// no meshing of flowing water -- TODO: allow meshing if max height? (I.e. if it's flowing straight down.)
	if (block_type == BlockType::FlowingWater) {
		return max_size;
	}

	// maximize height first, because it's better memory-wise
	for (int j = start_point[1] + 1, i = start_point[0]; j < 16; j++) {
		// if extended by 1, add 1 to max height
		if (layer[i][j] == block_type && !merged[i][j]) {
			max_size[1]++;
		}
		// else give up
		else {
			break;
		}
	}

	// maximize width now that we've maximized height

	// for each width
	bool stop = false;
	for (int i = start_point[0] + 1; i < 16; i++) {
		// check if entire height is correct
		for (int j = start_point[1]; j < start_point[1] + max_size[1]; j++) {
			// if wrong block type, give up on extending width
			if (layer[i][j] != block_type || merged[i][j]) {
				stop = true;
				break;
			}
		}

		if (stop) {
			break;
		}

		// yep, entire height is correct! Extend max width and keep going
		max_size[0]++;
	}

	return max_size;
}

MeshGenResult* WorldMeshPart::gen_minichunk_mesh_from_req(std::shared_ptr<MeshGenRequest> req) {
	// update invisibility
	bool invisible = req->data->self->all_air() || check_if_covered(req);

	// if visible, update mesh
	std::unique_ptr<MiniChunkMesh> non_water;
	std::unique_ptr<MiniChunkMesh> water;
	if (!invisible) {
		const std::unique_ptr<MiniChunkMesh> mesh = gen_minichunk_mesh(req);

		non_water = std::make_unique<MiniChunkMesh>();
		water = std::make_unique<MiniChunkMesh>();

		for (auto& quad : mesh->get_quads()) {
			if ((BlockType)quad.block == BlockType::StillWater || (BlockType)quad.block == BlockType::FlowingWater) {
				water->add_quad(quad);
			}
			else {
				non_water->add_quad(quad);
			}
		}

		assert(mesh->size() == non_water->size() + water->size());
	}

	// post result
	MeshGenResult* result = nullptr;
	if (non_water || water)
	{
		result = new MeshGenResult(req->data->self->get_coords(), invisible, std::move(non_water), std::move(water));
	}

	// generated result
	return result;
}

std::unique_ptr<MiniChunkMesh> WorldMeshPart::gen_minichunk_mesh(std::shared_ptr<MeshGenRequest> req) {
	// got our mesh
	std::unique_ptr<MiniChunkMesh> mesh = std::make_unique<MiniChunkMesh>();

	// for all 6 sides
	for (int i = 0; i < 6; i++) {
		bool backface = i < 3;
		int layers_idx = i % 3;

		// most efficient to traverse working_idx_1 then working_idx_2;
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// generate face variable
		vmath::ivec3 face = { 0, 0, 0 };
		// I don't think it matters whether we start with front or back face, as long as we switch halfway through.
		// BACKFACE => +X/+Y/+Z SIDE. 
		face[layers_idx] = backface ? -1 : 1;

		// for each layer
		for (int i = 0; i < 16; i++) {
			BlockType layer[16][16];
			bool merged[16][16];

			// extract it from the data
			gen_layer(req, layers_idx, i, face, layer);

			// get quads from layer
			vector<Quad2D> quads2d = gen_quads(layer, merged);

			// if -x, -y, or +z, flip triangles around so that we're not drawing them backwards
			if (face[0] < 0 || face[1] < 0 || face[2] > 0) {
				for (auto& quad2d : quads2d) {
					vmath::ivec2 diffs = quad2d.corners[1] - quad2d.corners[0];
					quad2d.corners[0][0] += diffs[0];
					quad2d.corners[1][0] -= diffs[0];
				}
			}

			// TODO: rotate texture sides the correct way. (It's noticeable when placing down diamond block.)
			// -> Or alternatively, can just rotate texture lmao.

			// convert quads back to 3D coordinates
			vector<Quad3D> quads = quads_2d_3d(quads2d, layers_idx, i, face);

			// if not backface (i.e. not facing (0,0,0)), move 1 forwards
			if (face[0] > 0 || face[1] > 0 || face[2] > 0) {
				for (auto& quad : quads) {
					quad.corner1 += face;
					quad.corner2 += face;
				}
			}

			// append quads
			for (auto quad : quads) {
				mesh->add_quad(quad);
			}
		}
	}

	return mesh;
}
