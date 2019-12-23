// UTIL class filled with useful static functions
#ifndef __UTIL_H__
#define __UTIL_H__

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <limits>
#include <map>
#include <numeric>
#include <tuple>
#include <vector>
#include <vmath.h>

static const vmath::vec4 NORTH_0 = vmath::vec4(0.0f, 0.0f, -1.0f, 0.0f);
static const vmath::vec4 NORTH_1 = vmath::vec4(0.0f, 0.0f, -1.0f, 1.0f);

static const vmath::vec4 SOUTH_0 = vmath::vec4(0.0f, 0.0f, 1.0f, 0.0f);
static const vmath::vec4 SOUTH_1 = vmath::vec4(0.0f, 0.0f, 1.0f, 1.0f);

static const vmath::vec4 EAST_0 = vmath::vec4(1.0f, 0.0f, 0.0f, 0.0f);
static const vmath::vec4 EAST_1 = vmath::vec4(1.0f, 0.0f, 0.0f, 1.0f);

static const vmath::vec4 WEST_0 = vmath::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
static const vmath::vec4 WEST_1 = vmath::vec4(-1.0f, 0.0f, 0.0f, 1.0f);

static const vmath::vec4 UP_0 = vmath::vec4(0.0f, 1.0f, 0.0f, 0.0f);
static const vmath::vec4 UP_1 = vmath::vec4(0.0f, 1.0f, 0.0f, 1.0f);

static const vmath::vec4 DOWN_0 = vmath::vec4(0.0f, -1.0f, 0.0f, 0.0f);
static const vmath::vec4 DOWN_1 = vmath::vec4(0.0f, -1.0f, 0.0f, 1.0f);

static const vmath::ivec4 INORTH_0 = vmath::ivec4(0, 0, -1, 0);
static const vmath::ivec4 ISOUTH_0 = vmath::ivec4(0, 0, 1, 0);
static const vmath::ivec4 IEAST_0 = vmath::ivec4(1, 0, 0, 0);
static const vmath::ivec4 IWEST_0 = vmath::ivec4(-1, 0, 0, 0);
static const vmath::ivec4 IUP_0 = vmath::ivec4(0, 1, 0, 0);
static const vmath::ivec4 IDOWN_0 = vmath::ivec4(0, -1, 0, 0);

static const vmath::ivec3 INORTH = vmath::ivec3(0, 0, -1);
static const vmath::ivec3 ISOUTH = vmath::ivec3(0, 0, 1);
static const vmath::ivec3 IEAST = vmath::ivec3(1, 0, 0);
static const vmath::ivec3 IWEST = vmath::ivec3(-1, 0, 0);
static const vmath::ivec3 IUP = vmath::ivec3(0, 1, 0);
static const vmath::ivec3 IDOWN = vmath::ivec3(0, -1, 0);

using namespace vmath;

// simple pair hash function
struct pair_hash
{
	template <class T1, class T2>
	std::size_t operator() (const std::pair<T1, T2> &pair) const
	{
		std::size_t seed = 0;
		hash_combine(seed, pair.first);
		hash_combine(seed, pair.second);
		return seed;
	}
};

// simple vecN hash function
struct vecN_hash
{
	template <typename T, const int len>
	std::size_t operator() (const vmath::vecN<T, len> vec) const
	{
		std::size_t seed = 0;

		for (int i = 0; i < len; i++) {
			hash_combine(seed, vec[i]);
		}

		return seed;
	}
};


// math sign function
template <typename T>
constexpr int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}

void print_arr(const GLfloat*, int, int);
GLuint compile_shaders(std::vector <std::tuple<std::string, GLenum>>);
GLuint link_program(GLuint);

// Create rotation matrix given pitch and yaw
inline mat4 rotate_pitch_yaw(float pitch, float yaw) {
	return
		rotate(pitch, vmath::vec3(1.0f, 0.0f, 0.0f)) * // rotate pitch around X
		rotate(yaw, vmath::vec3(0.0f, 1.0f, 0.0f));    // rotate yaw   around Y
}

inline vmath::ivec4 vec2ivec(vmath::vec4 v) {
	vmath::ivec4 result;
	for (int i = 0; i < v.size(); i++) {
		result[i] = (int)floorf(v[i]);
	}
	return result;
}

// given a vec3 index, generate the other 2 indices
static inline void gen_working_indices(int &layers_idx, int &working_idx_1, int &working_idx_2) {
	// working indices are always gonna be xy, xz, or yz.
	working_idx_1 = layers_idx == 0 ? 1 : 0;
	working_idx_2 = layers_idx == 2 ? 1 : 2;
}

static inline void gen_working_indices(int &&layers_idx, int &working_idx_1, int &working_idx_2) { return gen_working_indices(layers_idx, working_idx_1, working_idx_2); }

template <typename T, const int len>
static inline bool in_range(const vecN<T, len> &vec, const vecN<T, len> &min, const vecN<T, len> &max) {
	for (int i = 0; i < len; i++) {
		if (vec[i] < min[i] || vec[i] > max[i]) {
			return false;
		}
	}
	return true;
}

// Memory leak, delete returned result.
template <typename T, const int len>
static inline char* vec2str(vecN<T, len> vec) {
	int bufsize = vec.size() * 16 + 4;
	char* result = new char[bufsize];
	char* tmp = result;

	tmp += sprintf(tmp, "(");
	for (int i = 0; i < len; i++) {
		tmp += sprintf(tmp, "%d", vec[i]);
		if (i != (len - 1)) {
			tmp += sprintf(tmp, ", ");
		}
	}

	tmp += sprintf(tmp, ")");

	assert(tmp - result <= bufsize && "buffer overflow");

	return result;
}

// boost::hash_combine
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// check if sphere is inside frustrum planes
static inline bool sphere_in_frustrum(const vec3 &pos, const float radius, const vmath::vec4(&frustum_planes)[6]) {
	bool res = true;
	for (auto &plane : frustum_planes) {
		if (plane[0] * pos[0] + plane[1] * pos[1] + plane[2] * pos[2] + plane[3] <= -radius) {
			res = false;
		}
	}

	return res;
}

// extract planes from projection matrix
static inline void extract_planes_from_projmat(const vmath::mat4 &proj_mat, const vmath::mat4 &mv_mat, vmath::vec4(&planes)[6])
{
	const vmath::mat4 &mat = (proj_mat * mv_mat).transpose();

	planes[0] = mat[3] + mat[0];
	planes[1] = mat[3] - mat[0];
	planes[2] = mat[3] + mat[1];
	planes[3] = mat[3] - mat[1];
	planes[4] = mat[3] + mat[2];
	planes[5] = mat[3] - mat[2];
}

static inline void WindowsException(char *description) {
	MessageBox(NULL, description, "Thrown exception", MB_OK);
}

// generate all points in a circle a center
// TODO: cache
static inline std::vector<ivec2> gen_circle(int radius, ivec2 center = { 0, 0 }) {
	std::vector<ivec2> result;
	for (int x = -radius; x <= radius; x++) {
		for (int y = -radius; y <= radius; y++) {
			ivec2 coords = ivec2(x, y);
			if (length(coords) <= radius) {
				result.push_back(coords + center);
			}
		}
	}
	return result;
}

static inline std::vector<ivec2> gen_diamond(int radius, ivec2 center = { 0, 0 }) {
	std::vector<ivec2> result;
	for (int i = -radius; i <= radius; i++) {
		for (int j = -(radius - abs(i)); j <= radius - abs(i); j++) {
			result.push_back(center + ivec2(i, j));
		}
	}
	return result;
}

template <typename T>
static inline std::vector<int> argsort(int size, const T* data) {
	// initialize original indices
	std::vector<int> indices(size);
	std::iota(indices.begin(), indices.end(), 0);

	// sort indexes based on comparing values in data
	std::sort(indices.begin(), indices.end(), [&data](int i1, int i2) {return data[i1] < data[i2]; });

	return indices;
}

double noise2d(double x, double y);

// super fast x^p using Exponentiation by Squaring
// works for ints, doubles, you name it
// succeptible to overflows when used with ints/similar
template<typename T>
static constexpr inline T pown(T x, unsigned p) {
	T result = 1;

	while (p) {
		if (p & 0x1) {
			result *= x;
		}
		x *= x;
		p >>= 1;
	}

	return result;
}

// extract a piece of a texture atlas
// assuming uniform atlas -- i.e. every texture is the same size
//
// atlas_width		:	number of textures stored horizontally
// atlas_height		:	number of textures stored vertically
// tex_width		:	width  of each texture in pixels
// tex_height		:	height of each texture in pixels
// idx				:	index of texture piece in atlas
static constexpr inline void extract_from_atlas(float* atlas, unsigned atlas_width, unsigned atlas_height, unsigned components, unsigned tex_width, unsigned tex_height, unsigned idx, float* result) {
	// coordinates of top-left texture texel in the atlas
	unsigned start_x = (idx % atlas_width) * tex_width;
	unsigned start_y = (idx / atlas_width) * tex_height;

	// boom baby!
	for (int y = 0; y < tex_height; y++) {
		for (int x = 0; x < tex_width; x++) {
			for (int i = 0; i < components; i++) {
				result[(y * tex_width + x) * components + i] = atlas[((start_x + x) + (start_y + y) * (atlas_width * tex_width)) * components + i];
			}
		}
	}
}

// TODO: Could be slightly improved with `const`, `&`, and `private:` in the right places, but otherwise it works great.
template <typename K, typename V>
class interval_map
{
	std::map<K, V> my_map;
	typedef class std::map<K, V>::iterator iter;

	// fill with 0 by default
	interval_map() : interval_map(0) {
	}

	// create interval map with default v
	interval_map(V v) {
		my_map.insert(my_map.end(), { std::numeric_limits<K>::lowest(), v });
	}

	// map [begin, end) -> v
	// O(log N)
	void set_interval(K begin, K end, const V v) {
		if (begin >= end) return;

		// get interval that `begin` intersects with (inclusive)
		iter begin_intersect = --my_map.upper_bound(begin);

		// get interval that `end` intersects with (inclusive)
		iter end_intersect = --my_map.upper_bound(end);

		// if required, insert at start
		iter inserted_start = my_map.end();
		if (begin_intersect->second != v) {
			inserted_start = my_map.insert_or_assign(begin_intersect, begin, v);
		}

		// if required, insert at end
		iter inserted_end = my_map.end();
		if (end_intersect->second != v) {
			inserted_end = my_map.insert_or_assign(end_intersect, end, end_intersect->second);
		}

		// delete everyone inside
		iter del_start = inserted_start != my_map.end() ? inserted_start : begin_intersect;
		if (del_start->first == begin) {
			del_start++;
		}

		iter del_end = inserted_end != my_map.end() ? inserted_end : end_intersect;

		if (del_start != my_map.end()) {
			my_map.erase(del_start, del_end);
		}
	}

	// iterator which traverses elements in sorted order (smallest to largest)
	// O(1)
	constexpr inline iter &begin() {
		return my_map.begin();
	}

	// end of elements
	// O(1)
	constexpr inline iter &end() {
		return my_map.end();
	}

	// get iterator containing key `k`
	// O(log N)
	constexpr inline iter get_interval(K k) {
		return --my_map.upper_bound(k);
	}

	// get value at key `k`
	// O(log N)
	const V operator[](K const& k) {
		return (--my_map.upper_bound(k))->second;
	}
};

#endif // __UTIL_H__
