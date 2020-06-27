#pragma once

#include <functional>
#include <queue>
#include <unordered_set>
#include <utility>

// Method 1: Make a custom heap using pointers so that we can point map into there whose pointers don't get fucked when the heap is reorganized.
// Method 2: Use std::make_heap and stuff, but after calling the algorithm, repair the map (takes up to log(n) steps.)
// Method 3: Map keys to priorities. Keep priorities unique. (Add 0.0001 until it works.)
// Method 4: Just use std::priority queue, but disallow the user from erasing by key.
//           I think this works! Store pair<key, T>; pop key from set when deleting.

template <
	class T1,
	class T2,
	class Compare = std::less<T1>
>
struct FirstComparator
{
	Compare comp;
	bool operator()(const std::pair<T1, T2>& lhs, const std::pair<T1, T2>& rhs)
	{
		return comp(lhs.first, rhs.first);
	}
};

template <
	class T1,
	class T2,
	class Compare = std::less<T2>
>
struct SecondComparator
{
	Compare comp;
	bool operator()(const std::pair<T1, T2>& lhs, const std::pair<T1, T2>& rhs)
	{
		return comp(lhs.second, rhs.second);
	}
};

// This is like a priority queue, except each element gets a key to keep it unique
template<
	class Key,
	class T,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Compare = std::less<T>
>
// Extend priority_queue so we can iterate over its protected container
class unique_priority_queue : private std::priority_queue<typename std::pair<Key, T>, std::vector<std::pair<Key, T>>, SecondComparator<Key, T, Compare>>
{
private:
	using base_class = typename std::priority_queue<typename std::pair<Key, T>, std::vector<std::pair<Key, T>>, SecondComparator<Key, T, Compare>>;

public:
	using typename base_class::value_type;
	using typename base_class::const_reference;
	using typename base_class::size_type;

	using base_class::top;
	using base_class::size;

	unique_priority_queue() = default;
	virtual ~unique_priority_queue() = default;

	// No copying yet
	unique_priority_queue(unique_priority_queue&& other) = delete;
	unique_priority_queue& operator=(unique_priority_queue&& other) = delete;

	// You can swap though
	void swap(unique_priority_queue& other)
	{
		base_class::swap(other);
		std::swap(set, other.set);
	}

	void push(const value_type& pair)
	{
		const Key& key = pair.first;
		const T& value = pair.second;

		auto search = set.find(key);

		// If key does not exist
		if (search == set.end())
		{
			// Insert
			set.insert(key);
			base_class::push(pair);
		}
	}

	void pop()
	{
		const Key& key = base_class::top().first;
		set.erase(key);
		base_class::pop();
	}

	typename decltype(base_class::c)::const_iterator begin() noexcept
	{
		return base_class::c.begin();
	}

	typename decltype(base_class::c)::const_iterator end() noexcept
	{
		return base_class::c.end();
	}

private:
	// A set so that they're unique
	std::unordered_set<Key, Hash, KeyEqual> set;
};
