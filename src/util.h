// UTIL class filled with useful static functions
#ifndef __UTIL_H__
#define __UTIL_H__

#include "GL/gl3w.h"
#include "vmath.h"

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

constexpr vmath::vec4 NORTH_0 = vmath::vec4(0.0f, 0.0f, -1.0f, 0.0f);
static constexpr vmath::vec4 NORTH_1 = vmath::vec4(0.0f, 0.0f, -1.0f, 1.0f);

static constexpr vmath::vec4 SOUTH_0 = vmath::vec4(0.0f, 0.0f, 1.0f, 0.0f);
static constexpr vmath::vec4 SOUTH_1 = vmath::vec4(0.0f, 0.0f, 1.0f, 1.0f);

static constexpr vmath::vec4 EAST_0 = vmath::vec4(1.0f, 0.0f, 0.0f, 0.0f);
static constexpr vmath::vec4 EAST_1 = vmath::vec4(1.0f, 0.0f, 0.0f, 1.0f);

static constexpr vmath::vec4 WEST_0 = vmath::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
static constexpr vmath::vec4 WEST_1 = vmath::vec4(-1.0f, 0.0f, 0.0f, 1.0f);

static constexpr vmath::vec4 UP_0 = vmath::vec4(0.0f, 1.0f, 0.0f, 0.0f);
static constexpr vmath::vec4 UP_1 = vmath::vec4(0.0f, 1.0f, 0.0f, 1.0f);

static constexpr vmath::vec4 DOWN_0 = vmath::vec4(0.0f, -1.0f, 0.0f, 0.0f);
static constexpr vmath::vec4 DOWN_1 = vmath::vec4(0.0f, -1.0f, 0.0f, 1.0f);

static constexpr vmath::ivec4 INORTH_0 = vmath::ivec4(0, 0, -1, 0);
static constexpr vmath::ivec4 ISOUTH_0 = vmath::ivec4(0, 0, 1, 0);
static constexpr vmath::ivec4 IEAST_0 = vmath::ivec4(1, 0, 0, 0);
static constexpr vmath::ivec4 IWEST_0 = vmath::ivec4(-1, 0, 0, 0);
static constexpr vmath::ivec4 IUP_0 = vmath::ivec4(0, 1, 0, 0);
static constexpr vmath::ivec4 IDOWN_0 = vmath::ivec4(0, -1, 0, 0);

static constexpr vmath::ivec3 INORTH = vmath::ivec3(0, 0, -1);
static constexpr vmath::ivec3 ISOUTH = vmath::ivec3(0, 0, 1);
static constexpr vmath::ivec3 IEAST = vmath::ivec3(1, 0, 0);
static constexpr vmath::ivec3 IWEST = vmath::ivec3(-1, 0, 0);
static constexpr vmath::ivec3 IUP = vmath::ivec3(0, 1, 0);
static constexpr vmath::ivec3 IDOWN = vmath::ivec3(0, -1, 0);

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
constexpr int sgn(const T val) {
	return (T(0) < val) - (val < T(0));
}

void print_arr(const GLfloat*, int, int);
GLuint compile_shaders(const std::vector<std::tuple<std::string, GLenum>>& shader_fnames);
GLuint link_program(GLuint);

// Create rotation matrix given pitch and yaw
inline mat4 rotate_pitch_yaw(float pitch, float yaw) {
	return
		rotate(pitch, vmath::vec3(1.0f, 0.0f, 0.0f)) * // rotate pitch around X
		rotate(yaw, vmath::vec3(0.0f, 1.0f, 0.0f));    // rotate yaw   around Y
}

template <const int len>
inline vmath::vecN<int, len> vec2ivec(const vmath::vecN<float, len>& v) {
	vmath::vecN<int, len> result;
	for (int i = 0; i < len; i++) {
		result[i] = (int)floorf(v[i]);
	}
	return result;
}

// given a vec3 index, generate the other 2 indices
static inline void gen_working_indices(const int &layers_idx, int &working_idx_1, int &working_idx_2) {
	// working indices are always gonna be xy, xz, or yz.
	working_idx_1 = layers_idx == 0 ? 1 : 0;
	working_idx_2 = layers_idx == 2 ? 1 : 2;
}

template <typename T, const int len>
static inline bool in_range(const vecN<T, len> &vec, const vecN<T, len> &min, const vecN<T, len> &max) {
	for (int i = 0; i < len; i++) {
		if (vec[i] < min[i] || vec[i] > max[i]) {
			return false;
		}
	}
	return true;
}

template <typename T, const int len>
static inline std::unique_ptr<char[]> vec2str(vecN<T, len> vec, char* string_format_specifier = "%d") {
	assert(string_format_specifier != nullptr);

	const int bufsize = vec.size() * 16 + 4;
	auto result = std::make_unique<char[]>(bufsize);
	char* tmp = result.get();

	tmp += sprintf(tmp, "(");
	for (int i = 0; i < len; i++) {
		tmp += sprintf(tmp, string_format_specifier, vec[i]);
		if (i != (len - 1)) {
			tmp += sprintf(tmp, ", ");
		}
	}

	tmp += sprintf(tmp, ")");

	assert(tmp - result.get() <= bufsize && "buffer overflow");

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

// TODO: do this by starting from center and going out horizontally then vertically, like in that stackoverflow post
// TODO: maybe don't inline?
class CircleGenerator {
private:
	const int radius;

public:
	// define iterator class in here which inherits from std::iterator
	class iterator : public std::iterator<
		std::input_iterator_tag,	// iterator_category
		vmath::ivec2,				// value_type
		int,						// difference_type
		const vmath::ivec2*,		// pointer
		vmath::ivec2				// reference
	> {

		vmath::ivec2 xy;
		const int radius;

	private:
		inline bool is_end() const {
			return xy[0] == -radius && xy[1] == radius + 1;
		}

		inline float distance_to_center() const {
			const auto &x = xy[0];
			const auto &y = xy[1];

			return std::sqrt(x * x + y * y);
		}

	public:

		// construct iterator
		inline explicit iterator(const int radius, const int x, const int y) : radius(radius), xy({ x, y }) {
			// if we start out of range, start by moving in range
			if (distance_to_center() > radius) {
				(*this)++;
			}
		}

		// increment iterator
		inline iterator& operator++() {
			while (!is_end()) {
				auto &x = xy[0];
				auto &y = xy[1];

				// if reach the end of row, go back to start
				if (x == radius) {
					x = -radius;
					y++;
				}
				// continue down row
				else {
					x++;
				}

				if (distance_to_center() <= radius) {
					return *this;
				}
			}


			return *this;
		}

		// don't really get it but okay
		inline iterator operator++(int) {
			iterator retval = *this;
			++(*this);
			return retval;
		}

		// comparison operator
		inline bool operator==(iterator other) const {
			return xy == other.xy;
		}

		inline bool operator!=(iterator other) const { return !(*this == other); }

		// value
		inline reference operator*() const { return xy; }
	};

	inline CircleGenerator(const int radius) : radius(radius) {}

	inline iterator begin() const {
		// start at top-left corner
		return iterator(radius, -radius, -radius);
	}

	inline iterator end() const {
		// end just past bottom-left corner
		// requires that we increment x then y
		return iterator(radius, -radius, radius + 1);
	}
};

// generate all points in a circle a center
// TODO: cache
static inline std::vector<ivec2> gen_circle(const int radius, const ivec2 center = { 0, 0 }) {
	std::vector<ivec2> result;
	result.reserve(4 * radius * radius + 4 * radius + 1); // always makes <= (2r+1)^2 = 4r^2 + 4r + 1 elements

	CircleGenerator cg(radius);
	for (auto iter = cg.begin(); iter != cg.end(); iter++) {
		result.push_back(*iter + center);
	}

	return result;
}

static inline std::vector<ivec2> gen_diamond(const int radius, const vmath::ivec2 center = { 0, 0 }) {
	std::vector<ivec2> result(2 * radius * radius + 2 * radius + 1); // always makes 2r^2 + 2r + 1 elements

	int total_idx = 0;
	for (int i = -radius; i <= radius; i++) {
		for (int j = -(radius - abs(i)); j <= radius - abs(i); j++) {
			result[total_idx++] = center + ivec2(i, j);
		}
	}
	return result;
}

template <typename T>
static inline std::vector<int> argsort(const int size, const T* data) {
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


// In C++, -1 % 16 == -1. Want it to be 15 instead.
template<typename T>
static constexpr T posmod(const T &x, const T &m) {
	return ((x % m) + m) % m;
}

// TODO: Could be slightly improved with `const`, `&`, and `private:` in the right places, but otherwise it works great.
template <typename K, typename V>
class IntervalMap
{
private:
	std::map<K, V> my_map;

public:
	inline IntervalMap() : IntervalMap(0) {}

	// create interval map with default v
	inline IntervalMap(const V& v) {
		clear(v);
	}

	// map [begin, end) -> v
	// O(log N)
	void set_interval(const K& begin, const K& end, const V& v) {
		if (begin >= end) return;

		// get end intersector (inclusive)
		auto end_intersect = --my_map.upper_bound(end);

		// if required, insert at end
		auto inserted_end = my_map.end();
		if (end_intersect->second != v) {
			inserted_end = my_map.insert_or_assign(end_intersect, end, end_intersect->second);
		}

		// get begin intersector (inclusive)
		auto begin_intersect = --my_map.upper_bound(begin);

		// if required, insert at start
		auto inserted_start = my_map.end();
		if (begin_intersect->second != v) {
			inserted_start = my_map.insert_or_assign(begin_intersect, begin, v);
		}

		// delete everyone inside
		auto del_start = inserted_start != my_map.end() ? inserted_start : begin_intersect;
		if (del_start->first < begin || (del_start->first == begin && std::prev(del_start)->second != v)) {
			del_start++;
		}

		auto del_end = inserted_end != my_map.end() ? inserted_end : end_intersect;
		if (del_end != my_map.end() && del_end->first == end && std::next(del_end) != my_map.end() && del_end->second == v) {
			del_end++;
		}

		if (del_start != my_map.end() && del_start->first < del_end->first) {
			my_map.erase(del_start, del_end);
		}
	}

	// iterator which traverses elements in sorted order (smallest to largest)
	// O(1)
	inline auto begin() {
		return my_map.begin();
	}

	// end of elements
	// O(1)
	inline auto end() {
		return my_map.end();
	}

	// get iterator containing key `k`
	// O(log N)
	inline auto get_interval(K const& k) {
		return --my_map.upper_bound(k);
	}

	// get value at key `k`
	// O(log N)
	const inline V& operator[](K const& k) const {
		return (--my_map.upper_bound(k))->second;
	}

	// clear
	inline void clear(V const& v) {
		my_map.clear();
		my_map.insert(my_map.end(), { std::numeric_limits<K>::lowest(), v });
	}

	// get num intervals overall
	// always at least 1
	inline auto num_intervals() {
		return my_map.size();
	}

	// get number of intervals in a range
	// UNTESTED
	inline auto num_intervals(const K& start, const K& end) {
		return get_interval(end) - get_interval(start);
	}
};




#endif // __UTIL_H__
