#pragma once

#include <vector>
#include <functional>
#include "Murmur.h"

using std::vector;
using namespace std::placeholders;

template<typename T>
class BloomFilter {
private:
    vector<bool> _bits;
    vector<decltype(std::bind(MurmurHash64A, _1, _2, 10000))> _hashes;
    int _max;
    int _length;
    int _seed;
public:
    BloomFilter(int m, int n, int seed, int k = 4);

    auto add(const T &data);

    auto find(const T &data);

    virtual ~BloomFilter();
};

template<typename T>
BloomFilter<T>::BloomFilter(int m, int n, int seed, int k) :_length(m), _max(n), _seed(seed) {
    _bits = vector<bool>(m);
    for (int i = 0; i < k; i++) {
        _hashes.push_back(std::bind(MurmurHash64A, _1, _2, seed + i));
    }
}

template<typename T>
auto BloomFilter<T>::add(const T &data) {
    for (auto &_hash : _hashes) {
        auto h = _hash(&data, 4) % _length;
        _bits[h] = true;
    }
}

template<typename T>
auto BloomFilter<T>::find(const T &data) {
    auto result = true;
    for (auto &_hash : _hashes) {
        auto h = _hash(&data, 4) % _length;
        result = result && _bits[h];
    }
    return result;
}

template<typename T>
BloomFilter<T>::~BloomFilter() {
}
