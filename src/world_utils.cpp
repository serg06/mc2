#include "world_utils.h"

#include "messaging.h"

float intbound(const float s, const float ds)
{
	// Some kind of edge case, see:
	// http://gamedev.stackexchange.com/questions/47362/cast-ray-to-select-block-in-voxel-game#comment160436_49423
	const bool sIsInteger = round(s) == s;
	if (ds < 0 && sIsInteger) {
		return 0;
	}

	return (ds > 0 ? ceil(s) - s : s - floor(s)) / abs(ds);
}

void raycast(const vmath::vec4& origin, const vmath::vec4& direction, int radius,
	vmath::ivec3* result_coords, vmath::ivec3* result_face,
	const std::function <bool(const vmath::ivec3& coords, const vmath::ivec3& face)>& stop_check) {
	// From "A Fast Voxel Traversal Algorithm for Ray Tracing"
	// by John Amanatides and Andrew Woo, 1987
	// <http://www.cse.yorku.ca/~amana/research/grid.pdf>
	// <http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.42.3443>
	// Extensions to the described algorithm:
	//   • Imposed a distance limit.
	//   • The face passed through to reach the current cube is provided to
	//     the callback.

	// The foundation of this algorithm is a parameterized representation of
	// the provided ray,
	//                    origin + t * direction,
	// except that t is not actually stored; rather, at any given point in the
	// traversal, we keep track of the *greater* t values which we would have
	// if we took a step sufficient to cross a cube boundary along that axis
	// (i.e. change the integer part of the coordinate) in the variables
	// tMaxX, tMaxY, and tMaxZ

	// Cube containing origin point.
	int x = (int)floorf(origin[0]);
	int y = (int)floorf(origin[1]);
	int z = (int)floorf(origin[2]);
	// Break out direction vector.
	float dx = direction[0];
	float dy = direction[1];
	float dz = direction[2];
	// Direction to increment x,y,z when stepping.
	// TODO: Consider case where sgn = 0.
	int stepX = sgn(dx);
	int stepY = sgn(dy);
	int stepZ = sgn(dz);
	// See description above. The initial values depend on the fractional
	// part of the origin.
	float tMaxX = intbound(origin[0], dx);
	float tMaxY = intbound(origin[1], dy);
	float tMaxZ = intbound(origin[2], dz);
	// The change in t when taking a step (always positive).
	float tDeltaX = stepX / dx;
	float tDeltaY = stepY / dy;
	float tDeltaZ = stepZ / dz;
	// Buffer for reporting faces to the callback.
	vmath::ivec3 face = { 0, 0, 0 };

	// Avoids an infinite loop.
	if (dx == 0 && dy == 0 && dz == 0) {
		throw "Raycast in zero direction!";
	}

	// Rescale from units of 1 cube-edge to units of 'direction' so we can
	// compare with 't'.

	radius /= sqrt(dx * dx + dy * dy + dz * dz);

	//while (/* ray has not gone past bounds of world */
	//	(stepX > 0 ? x < wx : x >= 0) &&
	//	(stepY > 0 ? y < wy : y >= 0) &&
	//	(stepZ > 0 ? z < wz : z >= 0)) {

	while (/* ray has not gone past bounds of radius */
		true) {

		// Invoke the callback, unless we are not *yet* within the bounds of the
		// world.
		//if (!(x < 0 || y < 0 || z < 0 /*|| x >= wx || y >= wy || z >= wz*/)) {

		//}

		// success, set result values and return
		if (stop_check({ x, y, z }, face)) {
			*result_coords = { x, y, z };
			*result_face = face;
			return;
		}

		// tMaxX stores the t-value at which we cross a cube boundary along the
		// X axis, and similarly for Y and Z. Therefore, choosing the least tMax
		// chooses the closest cube boundary. Only the first case of the four
		// has been commented in detail.
		if (tMaxX < tMaxY) {
			if (tMaxX < tMaxZ) {
				if (tMaxX > radius) break;
				// Update which cube we are now in.
				x += stepX;
				// Adjust tMaxX to the next X-oriented boundary crossing.
				tMaxX += tDeltaX;
				// Record the normal vector of the cube face we entered.
				face[0] = -stepX;
				face[1] = 0;
				face[2] = 0;
			}
			else {
				if (tMaxZ > radius) break;
				z += stepZ;
				tMaxZ += tDeltaZ;
				face[0] = 0;
				face[1] = 0;
				face[2] = -stepZ;
			}
		}
		else {
			if (tMaxY < tMaxZ) {
				if (tMaxY > radius) break;
				y += stepY;
				tMaxY += tDeltaY;
				face[0] = 0;
				face[1] = -stepY;
				face[2] = 0;
			}
			else {
				// Identical to the second case, repeated for simplicity in
				// the conditionals.
				if (tMaxZ > radius) break;
				z += stepZ;
				tMaxZ += tDeltaZ;
				face[0] = 0;
				face[1] = 0;
				face[2] = -stepZ;
			}
		}
	}
	// nothing found, set invalid results and return
	*result_coords = { 0, -1, 0 }; // invalid
	*result_face = { 0, 0, 0 }; // invalid
	return;
}


bool operator==(const Quad2D& lhs, const Quad2D& rhs) {
	auto& lc1 = lhs.corners[0];
	auto& lc2 = lhs.corners[1];

	auto& rc1 = rhs.corners[0];
	auto& rc2 = rhs.corners[1];

	return
		(lhs.block == rhs.block) &&
		((lc1 == rc1 && lc2 == rc2) || (lc2 == rc1 && lc1 == rc2));
}

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const int x, const int z) {
	return get_chunk_coords((float)x, (float)z);
}

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const float x, const float z) {
	return { (int)floorf(x / 16.0f), (int)floorf(z / 16.0f) };
}

// get minichunk-coordinates of minichunk containing the block at (x, y, z)
vmath::ivec3 get_mini_coords(const int x, const int y, const int z) {
	return { (int)floorf((float)x / 16.0f), (y / 16) * 16, (int)floorf((float)z / 16.0f) };
}

vmath::ivec3 get_mini_coords(const vmath::ivec3& xyz) { return get_mini_coords(xyz[0], xyz[1], xyz[2]); }

// given a block's real-world coordinates, return that block's coordinates relative to its chunk
vmath::ivec3 get_chunk_relative_coordinates(const int x, const int y, const int z) {
	return vmath::ivec3(posmod(x, CHUNK_WIDTH), y, posmod(z, CHUNK_DEPTH));
}

// given a block's real-world coordinates, return that block's coordinates relative to its mini
vmath::ivec3 get_mini_relative_coords(const int x, const int y, const int z) {
	return vmath::ivec3(posmod(x, MINICHUNK_WIDTH), y % MINICHUNK_HEIGHT, posmod(z, MINICHUNK_DEPTH));
}

vmath::ivec3 get_mini_relative_coords(const vmath::ivec3& xyz) { return get_mini_relative_coords(xyz[0], xyz[1], xyz[2]); }
