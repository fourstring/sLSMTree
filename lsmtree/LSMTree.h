//
// Created by fourstring on 2020/4/4.
//

#ifndef LSMTREE_LSMTREE_H
#define LSMTREE_LSMTREE_H

#include "../memtable/MemTable.h"
#include "../disktable/DiskTable.h"

class LSMTree {
private:
    MemTable *memory;
    DiskTable *disk;
    path data_home;
    const int MEMTABLE_LIMIT = 2 * 1000 * 1000;
public:
    explicit LSMTree(path &data_dir);

    ~LSMTree();

    std::string get(long long key);

    void put(long long key, const std::string &s);

    bool del(long long key);

    void reset();
};


#endif //LSMTREE_LSMTREE_H
