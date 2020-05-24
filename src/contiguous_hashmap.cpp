#include "contiguous_hashmap.h"
#include "util.h"
#include "vmath.h"

#include <type_traits>

void test()
{
	contiguous_hashmap<vmath::ivec2, int*, vecN_hash> map;
}
