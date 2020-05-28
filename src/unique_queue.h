#pragma once

#include <functional>
#include <iostream>
#include <list>
#include <unordered_map>
#include <utility>

template<
	class Key,
	class T,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>
>
class unique_queue
{
public:
	using value_type = std::pair<const Key, T>;
	using reference = value_type&;
	using const_reference = const value_type&;
	using size_type = typename std::list<T>::size_type;

	reference front()
	{
		return list.front();
	}

	const_reference front() const
	{
		return list.front();
	}

	void push_back(const value_type& pair)
	{
		const Key& key = pair.first;
		const T& value = pair.second;

		auto search = map.find(key);

		// if key exists
		if (search != map.end())
		{
			// move to back of queue
			list.erase(search->second);
			list.push_back(pair);

			// update map
			search->second = std::prev(list.end());
		}

		// if key doesn't exist
		else
		{
			// add to back of queue
			list.push_back(pair);

			// update map
			map[key] = std::prev(list.end());
		}
	}

	void push_front(const value_type& pair)
	{
		const Key& key = pair.first;
		const T& value = pair.second;

		auto search = map.find(key);

		// if key exists
		if (search != map.end())
		{
			// move to back of queue
			list.erase(search->second);
			list.push_front(pair);

			// update map
			search->second = list.begin();
		}

		// if key doesn't exist
		else
		{
			// add to back of queue
			list.push_front(pair);

			// update map
			map[key] = list.begin();
		}
	}

	// TODO: implement these when originals are more stable
	void push_back(value_type&& pair) { push_back(pair); };
	void push_front(value_type&& pair) { push_front(pair); };

	void pop()
	{
		const Key& key = list.front().first;
		map.erase(key);
		list.pop_front();
	}

	size_type size() const
	{
		return list.size();
	}

	// for debugging
	void print_inorder()
	{
		std::cout << "[";
		for (auto iter = list.begin(); iter != list.end(); iter++)
		{
			std::cout << iter->second;
			if (std::next(iter) != list.end())
			{
				std::cout << ", ";
			}
		}
		std::cout << "]\n";
	}

	// for debugging
	void print_with_keys()
	{
		std::cout << "[";
		for (auto iter = map.begin(); iter != map.end(); iter++)
		{
			std::cout << "(" << iter->first << ", " << (*(iter->second)).second << ")";
			if (std::next(iter) != map.end())
			{
				std::cout << ", ";
			}
		}
		std::cout << "]\n";
	}

private:
	// A queue so that we know which ones came first
	std::list<value_type> list;

	// A set so that they're unique
	std::unordered_map<Key, typename std::list<value_type>::iterator, Hash> map;
};
