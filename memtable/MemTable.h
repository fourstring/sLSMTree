//
// Created by fourstring on 2020/4/4.
//

#ifndef LSMTREE_MEMTABLE_H
#define LSMTREE_MEMTABLE_H

#include "../skip_list/SkipList.h"
#include "../skip_list/QuadList.h"
#include "../Dictionary.h"
#include "../disktable/sstable/SSTable.h"
#include <ctime>
#include <random>
#include <iterator>
#include <list>
#include <string>

class MemTable : public Dictionary<long long, std::string> {
protected:
    using MemTableEntry=SSTableDataEntry;
    using MemTableLevel = QuadList<MemTableEntry>;
    using LevelIter = typename std::list<MemTableLevel>::iterator;
    using NodePosi=QuadListNode<MemTableEntry> *;

    std::list<MemTableLevel> qlist;

    auto skipSearch(LevelIter level, long long key);

    bool _put(NodePosi pred_node, long long key, std::string value, bool delete_flag);

    static size_t size_of_node(NodePosi node);

    long long _size = 0;

    size_t _size_bytes = 0;
public:
    explicit MemTable();

    MemTable(MemTable &&m) noexcept;

    [[nodiscard]] long long size() const override;

    bool put(long long k, std::string v) override;

    std::string *get(long long k) override;

    bool remove(long long k) override;

    [[nodiscard]] int levels() const {
        return qlist.size();
    }

    size_t size_bytes();

    SSTableData collectData();
};


#endif //LSMTREE_MEMTABLE_H
