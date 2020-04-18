//
// Created by fourstring on 2020/4/14.
//
#include "gzip/compress.hpp"
#include "gzip/decompress.hpp"
#include <string>
#include <iostream>
#include <random>
#include <ctime>
#include <cstring>

int main() {
    auto seed = time(nullptr);
    auto gen = std::mt19937_64{static_cast<unsigned long long >(seed)};
    auto rand = std::uniform_int_distribution(0, 25);
    std::string test;
    for (int i = 0; i < 65536; ++i) {
        test.push_back('a' + rand(gen));
    }

    std::string compressed = gzip::compress(test.data(), test.size());
    std::cout << compressed.length() << ' ' << compressed.size();
    std::string decompressed = gzip::decompress(compressed.data(), compressed.size());
}
