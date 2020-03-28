#pragma once

#include <cstdint>

uint64_t MurmurHash64A(const void *key, int len, unsigned int seed);

uint64_t MurmurHash64B(const void *key, int len, unsigned int seed);