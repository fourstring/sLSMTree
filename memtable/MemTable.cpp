//
// Created by fourstring on 2020/4/4.
//

#include "MemTable.h"

#include <cstring>
#include <utility>

MemTable::MemTable() : qlist() {
    qlist.emplace_back();
}

MemTable::MemTable(MemTable &&m) noexcept {
    qlist = std::move(m.qlist);
    _size_bytes = m._size_bytes;
    _size = m._size;
    m._size_bytes = 0;
    m._size = 0;
}

auto MemTable::skipSearch(MemTable::LevelIter level, long long key) {
    struct Result {
        bool valid;
        decltype(level->first()->succ) node;
    };
    auto *curr_node = level->first()->succ;
    auto result = Result{false, nullptr};
    while (true) {
        while (level->valid(curr_node) && curr_node->data.key <= key)
            curr_node = curr_node->succ;
        curr_node = curr_node->pred;
        result.node = curr_node;
        if (curr_node->data.key == key && level->valid(curr_node)) {
            result.valid = true;
            while (result.node->below) {
                result.node = result.node->below;
                // If k found, move to the bottom of its tower for better removing.
                // Though it has been marked as deleted, it should be return for further use like put.
                // This loop unable to process curr_node is header or trailer of level.
            }
            /*if (!level->valid(curr_node)) {
                if (curr_node == level->first()) {
                    result.node = qlist.rbegin()->first();
                }
                if (curr_node == level->last()) {
                    result.node = qlist.rbegin()->last();
                }
            }*/
            return result;
        }
        auto r = --qlist.rbegin().base();
        if (level == r) break;
        level++;
        if (curr_node->below) { // header and trailer node of a QuadList has no above or below. Their tower can be formed by qlist.
            curr_node = curr_node->below;
        } else {
            curr_node = level->first()->succ;
        }
    }

    return result;
}

bool MemTable::_put(NodePosi pred_node, long long key, std::string value, bool delete_flag) {
    auto curr_level = qlist.rbegin(); // If skipSearch didn't find k, it must return a appropriate position
    // at the bottom of qlist for insert a new tower of k
    static auto seed = time(nullptr);
    static auto gen = std::mt19937_64{static_cast<unsigned long long> (seed)};
    static auto rand = std::uniform_int_distribution<>{0, 1};
    auto timestamp = time(nullptr);
    auto new_node = curr_level->insertAfterAbove(MemTableEntry{delete_flag, timestamp, key, std::move(value)},
                                                 pred_node);
    _size_bytes += size_of_node(new_node);
    while (rand(gen)) {
        while (!pred_node->above && curr_level->valid(pred_node))
            pred_node = pred_node->pred;
        if (pred_node->above) {
            // Find a node of a tower.
            pred_node = pred_node->above;
            curr_level++;
        } else {
            // Unable to find a tower higher than curr_level.
            if (--(curr_level.base()) == qlist.begin()) {
                // If the tower should grow though top of SkipList arrived, add a level as top
                qlist.push_front(MemTableLevel{});
            }
            curr_level++;
            pred_node = curr_level->first();
        }
        // Do not repeat data on upper node to save memory, because skipSearch always turn to bottom if hit.
        new_node = curr_level->insertAfterAbove(MemTableEntry{delete_flag, 0, key, ""}, pred_node, new_node);
    }
    _size++;
    return true;
}

long long MemTable::size() const {
    return _size;
}

bool MemTable::put(long long k, std::string v) {
    auto[valid, node]=skipSearch(qlist.begin(), k);
    auto timestamp = time(nullptr);
    if (!valid) {
        // k don't exists. Insert new tower.
        _put(node, k, v, false);
    }
    if (node->data.key == k && !node->data.delete_flag) {
        _size_bytes -= size_of_node(node);
        node->data.value_length = std::strlen(v.data());
        node->data.value = std::move(v);
        node->data.timestamp = timestamp;
        _size_bytes += size_of_node(node);
        return true;
    }
    if (node->data.key == k && node->data.delete_flag) {
        // k found, but marked as deleted
        // update value, timestamp and reset delete_flag.
        _size_bytes -= size_of_node(node);
        node->data.value_length = std::strlen(v.data());
        node->data.value = std::move(v);
        node->data.timestamp = timestamp;
        _size_bytes += size_of_node(node);
        do {
            node->data.delete_flag = false;
            node = node->above;
        } while (node != nullptr);
        return true;
    }
    return false;
}

std::string *MemTable::get(long long k) {
    if (qlist.empty()) return nullptr;
    auto[valid, res] = skipSearch(qlist.begin(), k);
    return res->data.key == k && valid ? &(res->data.value) : nullptr;
}

bool MemTable::remove(long long k) {
    auto[valid, to_delete] = skipSearch(qlist.begin(), k);
    auto timestamp = time(nullptr);
    if (!valid) {
        // k not found, as MemTable of a LSMTree, insert new tower marked as deleted.
        _put(to_delete, k, "", true);
    }
    if (to_delete->data.key == k && !to_delete->data.delete_flag) {
        // found k and has not been marked as deleted. mark deleted, remove value, update timestamp.
        _size_bytes -= size_of_node(to_delete);
        to_delete->data.value = "";
        to_delete->data.timestamp = timestamp;
        to_delete->data.value_length = 0;
        _size_bytes += size_of_node(to_delete);
        do {
            to_delete->data.delete_flag = true;
            to_delete = to_delete->above;
        } while (to_delete != nullptr);
        return true;
    }
    if (to_delete->data.key == k && to_delete->data.delete_flag) {
        // found k and its has been marked as deleted, just update timestamp.
        to_delete->data.timestamp = timestamp; // bytes level size don't change.
    }
    return true;
}

size_t MemTable::size_of_node(MemTable::NodePosi node) {
    if (node == nullptr) {
        return 0;
    }
    return sizeof(bool) + sizeof(time_t) + sizeof(long long) + sizeof(size_t) + std::strlen(node->data.value.c_str());
}

SSTableData MemTable::collectData() {
    auto new_data = SSTableData{};
    auto *bottomStart = (--qlist.end())->first()->succ;
    auto *bottomEnd = (--qlist.end())->last();
    while (bottomStart != bottomEnd) {
        new_data.push_back(std::move(bottomStart->data));
        bottomStart = bottomStart->succ;
    }
    return new_data;
}

size_t MemTable::size_bytes() {
    return _size_bytes;
}

