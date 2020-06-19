#pragma once

#include <functional>
#include <iostream>
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
class unique_priority_queue
{
public:
	using value_type = std::pair<Key, T>;
	using reference = value_type&;
	using const_reference = const value_type&;

private:
	// The priority queue
	std::priority_queue<typename value_type, std::vector<value_type>, SecondComparator<Key, T, Compare>> pq;

	// A set so that they're unique
	std::unordered_set<Key, Hash, KeyEqual> set;

public:
	using size_type = typename decltype(pq)::size_type;

	unique_priority_queue() = default;
	virtual ~unique_priority_queue() = default;

	// No copying yet
	unique_priority_queue(unique_priority_queue&& other) = delete;
	unique_priority_queue& operator=(unique_priority_queue&& other) = delete;

	// You can swap though
	void swap(unique_priority_queue& other)
	{
		std::swap(pq, other.pq);
		std::swap(set, other.set);
	}

	const_reference top() const
	{
		return pq.top();
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
			pq.push(pair); // TODO: FIX
		}
	}

	// TODO: implement when original is more stable
	void push(value_type&& pair) { push_back(pair); };

	void pop()
	{
		const Key& key = pq.top().first;
		set.erase(key);
		pq.pop();
	}

	size_type size() const
	{
		return pq.size();
	}

//	// for debugging
//	void print_inorder()
//	{
//#ifdef _DEBUG
//		std::stringstream s;
//		s << "[";
//		for (auto iter = list.begin(); iter != list.end(); ++iter)
//		{
//			s << iter->second;
//			if (std::next(iter) != list.end())
//			{
//				s << ", ";
//			}
//		}
//		s << "]\n";
//		OutputDebugString(s.str().c_str());
//#endif // _DEBUG
//	}
//
//	// for debugging
//	void print_with_keys()
//	{
//#ifdef _DEBUG
//		std::stringstream s;
//		s << "[";
//		for (auto iter = map.begin(); iter != map.end(); ++iter)
//		{
//			s << "(" << iter->first << ", " << (*(iter->second)).second << ")";
//			if (std::next(iter) != map.end())
//			{
//				s << ", ";
//			}
//		}
//		s << "]\n";
//		OutputDebugString(s.str().c_str());
//#endif // _DEBUG
//	}
};
