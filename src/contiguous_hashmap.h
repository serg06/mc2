#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

// Inefficiency: Two keys per value.
template<
    class Key,
    class T,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>
>
class contiguous_hashmap
{
public:
    using size_type = typename std::vector<T>::size_type;
    using value_iterator = typename std::vector<T>::const_iterator;

    contiguous_hashmap() = default;
    virtual ~contiguous_hashmap() = default;

    // Too lazy to implement moving/copying at the moment
    contiguous_hashmap(const contiguous_hashmap&) = delete;
    contiguous_hashmap& operator=(const contiguous_hashmap&) = delete;

    // By default iterate over vector (fast)
    value_iterator begin() const noexcept
    {
        return vec.begin();
    }

    value_iterator end() const noexcept
    {
        return vec.end();
    }

    // unordered_map::find
    value_iterator find(const Key& key) const
    {
        const auto search = map.find(key);
        if (search == map.end())
        {
            return end();
        }
        else
        {
            return vec.begin() + search->second;
        }
    }

    T& operator[](Key&& key)
    {
        const auto search = map.find(key);

        // If doesn't exist, insert and return reference
        if (search == map.end())
        {
            vec.push_back({});
            map[std::move(key)] = vec.size()-1;
            return vec.back()->second;
        }

        // If does exist, just return reference
        else
        {
            return vec[*search];
        }
    }

    T& operator[](const Key& key)
    {
        const auto search = map.find(key);

        // If doesn't exist, insert and return reference
        if (search == map.end())
        {
            vec.push_back({});
            map[key] = vec.size() - 1;
            return vec.back();
        }

        // If does exist, just return reference
        else
        {
            return vec[search->second];
        }
    }

private:
    std::unordered_map <Key, size_type, Hash, KeyEqual> map;
    std::vector<T> vec;
};
