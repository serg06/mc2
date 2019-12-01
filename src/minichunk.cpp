#include "minichunk.h"

#include <initializer_list>
#include <vmath.h>

std::vector <ivec3> surrounding_minichunks(ivec3 minichunk_coord) {
	std::vector<ivec3> result = {
		// horizontal sides
		minichunk_coord + ivec3(1, minichunk_coord[1], 0),
		minichunk_coord + ivec3(0, minichunk_coord[1], 1),
		minichunk_coord + ivec3(-1, minichunk_coord[1], 0),
		minichunk_coord + ivec3(0, minichunk_coord[1], -1),
	};

	// if not is top minichunk, add the one above it
	if (minichunk_coord[1] < (CHUNK_HEIGHT - MINICHUNK_HEIGHT - 1)) {
		result.push_back(minichunk_coord + ivec3(0, minichunk_coord[1] + 1, 0));
	}

	// if not is bottom minichunk, add the below above it
	if (minichunk_coord[1] > 0) {
		result.push_back(minichunk_coord + ivec3(0, minichunk_coord[1] - 1, 0));
	}

	return result;
}