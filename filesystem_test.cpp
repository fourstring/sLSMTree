//
// Created by fourstring on 2020/4/4.
//
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <iostream>

using namespace std::filesystem;
using namespace std;

int main() {
    auto i = directory_iterator(current_path());
    auto v = vector<directory_entry>{};
    for (auto &e:i) {
        v.push_back(e);
    }
    stable_sort(v.begin(), v.end());
    for (auto &m:v) {
        cout << m << endl;
    }
    if (!exists("./data/0")) {
        cout << "NMSL:";
    }
}