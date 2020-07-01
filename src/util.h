#pragma once

#include "GL/glcorearb.h"
#include "imgui.h"
#include "vmath.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <tuple>
#include <vector>


static constexpr vmath::vec4 NORTH_0 = vmath::vec4(0.0f, 0.0f, -1.0f, 0.0f);
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

void print_arr(const GLfloat*, int, int);
GLuint link_program(GLuint);
GLuint compile_shaders(const std::vector<std::tuple<std::string, GLenum>>& shader_fnames);

// Create rotation matrix given pitch and yaw
vmath::mat4 rotate_pitch_yaw(float pitch, float yaw);

// extract planes from projection matrix
void extract_planes_from_projmat(const vmath::mat4& proj_mat, const vmath::mat4& mv_mat, vmath::vec4(&planes)[6]);

// check if sphere is inside frustum planes
bool sphere_in_frustum(const vmath::vec3& pos, const float radius, const vmath::vec4(&frustum_planes)[6]);

void WindowsException(const char* description);

// generate all points in a circle a center
// TODO: cache
// TODO: return std::array using -> to specify return type?
std::vector<vmath::ivec2> gen_circle(const int radius, const vmath::ivec2 center = { 0, 0 });

std::vector<vmath::ivec2> gen_diamond(const int radius, const vmath::ivec2 center = { 0, 0 });

// extract a piece of a texture atlas
// assuming uniform atlas -- i.e. every texture is the same size
//
// atlas_width		:	number of textures stored horizontally
// atlas_height		:	number of textures stored vertically
// tex_width		:	width  of each texture in pixels
// tex_height		:	height of each texture in pixels
// idx				:	index of texture piece in atlas
void extract_from_atlas(float* atlas, unsigned atlas_width, unsigned atlas_height, unsigned components, unsigned tex_width, unsigned tex_height, unsigned idx, float* result);

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

	public:
		explicit iterator(const int radius, const int x, const int y);
		iterator& operator++();
		iterator operator++(int);
		bool operator==(iterator other) const;
		bool operator!=(iterator other) const;
		reference operator*() const;

	private:
		bool is_end() const;
		float distance_to_center() const;

	private:
		vmath::ivec2 xy;
		const int radius;
	};

	inline CircleGenerator(const int radius);
	inline iterator begin() const;
	inline iterator end() const;
};

// simple pair hash function
struct pair_hash
{
	template <class T1, class T2>
	std::size_t operator() (const std::pair<T1, T2>& pair) const
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

template <const int len>
inline vmath::vecN<int, len> vec2ivec(const vmath::vecN<float, len>& v) {
	vmath::vecN<int, len> result;
	for (int i = 0; i < len; i++) {
		result[i] = (int)floorf(v[i]);
	}
	return result;
}

template <typename T, const int len>
static inline bool in_range(const vmath::vecN<T, len>& vec, const vmath::vecN<T, len>& min, const vmath::vecN<T, len>& max) {
	for (int i = 0; i < len; i++) {
		if (vec[i] < min[i] || vec[i] > max[i]) {
			return false;
		}
	}
	return true;
}

template <typename T, const int len>
inline std::string vec2str(vmath::vecN<T, len> vec) {
	std::stringstream out;
	out << "(";
	for (int i = 0; i < len; i++) {
		out << vec[i];
		if (i != (len - 1)) {
			out << ", ";
		}
	}
	out << ")";
	return out.str();
}

// boost::hash_combine
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
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

// In C++, -1 % 16 == -1. Want it to be 15 instead.
template<typename T>
static constexpr T posmod(const T& x, const T& m) {
	if constexpr (std::is_integral_v<T>)
	{
		return ((x % m) + m) % m;
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		return fmod(fmod(x, m) + m, m);
	}
	else
	{
		// Not supported
		static_assert(1 != 0);
	}
}

// Thread-safe random number generator.
// Each thread's generator is guaranteed to get a unique seed.
template<typename T = int32_t>
T rand_32(const T& min = (std::numeric_limits<T>::min)(), const T& max = (std::numeric_limits<T>::max)())
{
	static thread_local std::mt19937* generator = nullptr;
	if (!generator)
	{
		static std::atomic_uint64_t extra(0);

		// TODO: Learn memory order
		int toadd = extra.fetch_add(1);

		auto now = std::chrono::high_resolution_clock::now();

		// TODO: Figure out the type of this
		auto test = now.time_since_epoch().count();

		// wew
		generator = new std::mt19937(test + toadd);
	}

	std::uniform_int_distribution<T> distribution(min, max);
	return distribution(*generator);
}

// Thread-safe random number generator.
// Each thread's generator is guaranteed to get a unique seed.
template<typename T = int64_t>
T rand_64(const T& min = (std::numeric_limits<T>::min)(), const T& max = (std::numeric_limits<T>::max)())
{
	static thread_local std::mt19937_64* generator = nullptr;
	if (!generator)
	{
		static std::atomic_uint64_t extra(0);

		// TODO: Learn memory order
		int toadd = extra.fetch_add(1);

		auto now = std::chrono::high_resolution_clock::now();

		// TODO: Figure out the type of this
		auto test = now.time_since_epoch().count();

		// wew
		generator = new std::mt19937_64(test + toadd);
	}

	std::uniform_int_distribution<T> distribution(min, max);
	return distribution(*generator);
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

template<
	class T,
	class Container = std::vector<T>,
	class Compare = std::less<typename Container::value_type>
>
class priority_queue_hacker : public std::priority_queue<T, Container, Compare>
{
public:
	Container& get_c()
	{
		return c;
	}

	Compare& get_comp()
	{
		return comp;
	}
};

template<class T, class Container, class Compare>
void update_pq_priorities(std::priority_queue<T, Container, Compare>& pq, std::function<void(T&)>& update)
{
	// Get container
	priority_queue_hacker<T, Container, Compare> hacker;
	hacker.swap(pq);
	Container& c = hacker.get_c();

	// Update priorities
	for (auto& entry : c)
	{
		update(entry);
	}

	// Fix heap
	std::make_heap(c.begin(), c.end(), hacker.get_comp());

	// Stick it back in
	pq.swap(hacker);
}

// For ImGui
ImVec2 max_btn_size(std::vector<std::string>& btnTexts);
