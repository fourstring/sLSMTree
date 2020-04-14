//
// Created by fourstring on 2020/4/4.
//

#include "LSMTree.h"

LSMTree::LSMTree(path &data_dir) {
    memory = new MemTable{};
    disk = new DiskTable{data_dir};
    data_home = data_dir;
}

std::string LSMTree::get(long long key) {
    auto memory_result = memory->get(key);
    if (memory_result != nullptr) {
        return *memory_result;
    } else {
        auto[success, disk_result]=disk->get(key);
        if (success) {
            return disk_result;
        } else {
            return "";
        }
    }
}

void LSMTree::put(long long key, const std::string &s) {
    memory->put(key, s);
    if (memory->size_bytes() > MEMTABLE_LIMIT) {
        disk->persistent(std::move(*memory));
        memory = new MemTable{};
    }
}

bool LSMTree::del(long long key) {
    if (get(key).empty()) {
        return false;
    }
    memory->remove(key);
    if (memory->size_bytes() > MEMTABLE_LIMIT) {
        disk->persistent(std::move(*memory), true);
        memory = new MemTable{};
    }
    return true;
}

void LSMTree::reset() {
    delete memory;
    delete disk;
    remove_all(data_home);
    memory = new MemTable{};
    disk = new DiskTable{data_home};
}

LSMTree::~LSMTree() {
    if (memory->size() != 0) {
        disk->persistent(std::move(*memory));
    }
    delete memory;
    delete disk;
}
