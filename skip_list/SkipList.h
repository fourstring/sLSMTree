//
// Created by fourstring on 2020/3/2.
//

#ifndef SKIP_LIST_SKIPLIST_H
#define SKIP_LIST_SKIPLIST_H

#include <ctime>
#include <random>
#include <iterator>
#include <list>
#include "QuadList.h"
#include "../Dictionary.h"

template<typename K, typename V>
class SkipList : public Dictionary<K, V> {
protected:
    using SkipListLevel = QuadList<Entry<K, V>>;
    using LevelIter = typename std::list<SkipListLevel>::iterator;

    std::list<SkipListLevel> qlist;

    auto skipSearch(LevelIter level, K key);

    int _size = 0;

public:
    SkipList();

    [[nodiscard]] int size() const override;

    bool put(K k, V v) override;

    V *get(K k) override;

    bool remove(K k) override;

    [[nodiscard]] int levels() const {
        return qlist.size();
    }
};

template<typename K, typename V>
int SkipList<K, V>::size() const {
    return _size;
}

template<typename K, typename V>
bool SkipList<K, V>::put(K k, V v) {
    auto[success, pred_node] = skipSearch(qlist.begin(), k);
    if (success) return true;
    auto curr_level = qlist.rbegin(); // If skipSearch didn't find k, it must return a appropriate position
    // at the bottom of qlist for insert a new tower of k
    auto seed = time(nullptr);
    auto gen = std::mt19937_64{static_cast<unsigned long long> (seed)};
    auto rand = std::uniform_int_distribution<>{0, 1};
    auto new_node = curr_level->insertAfterAbove(Entry<K, V>{k, v}, pred_node);

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
                qlist.push_front(SkipListLevel{});
            }
            curr_level++;
            pred_node = curr_level->first();
        }
        new_node = curr_level->insertAfterAbove(Entry<K, V>{k, v}, pred_node, new_node);
    }
    _size++;
    return true;
}

template<typename K, typename V>
V *SkipList<K, V>::get(K k) {
    if (qlist.empty()) return nullptr;
    auto[success, res] = skipSearch(qlist.begin(), k);
    return success ? &(res->data.value) : nullptr;
}

template<typename K, typename V>
bool SkipList<K, V>::remove(K k) {
    auto[success, to_delete] = skipSearch(qlist.begin(), k);
    if (!success) return false;
    auto above = to_delete->above;
    auto curr_level = qlist.rbegin();
    while (true) {
        curr_level->remove(to_delete);
        if (curr_level->empty()) {
            // if the level of to_delete was emptied after deleting, remove the level.
            auto curr_level_iter = --curr_level.base();
            qlist.erase(curr_level_iter);
        }
        if (!above) break;
        curr_level++;
        to_delete = above;
        above = to_delete->above;
    }
    return true;
}

template<typename K, typename V>
auto SkipList<K, V>::skipSearch(SkipList::LevelIter level, K key) {
    struct Result {
        bool success;
        decltype(level->first()->succ) node;
    };
    auto *curr_node = level->first()->succ;
    auto result = Result{false, curr_node};
    while (true) {
        while (level->valid(curr_node) && curr_node->data.key <= key)
            curr_node = curr_node->succ;
        curr_node = curr_node->pred;
        result.node = curr_node;
        if (curr_node->data.key == key) {
            result.success = true;
            while (result.node->below) {
                result.node = result.node->below;// If k found, move to the bottom of its tower for better removing.
            }
            break;
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

template<typename K, typename V>
SkipList<K, V>::SkipList() {
    this->qlist.push_front(SkipListLevel{});
}


#endif //SKIP_LIST_SKIPLIST_H
