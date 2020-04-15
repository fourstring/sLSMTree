//
// Created by fourstring on 2020/4/4.
//

#ifndef LSMTREE_DISKTABLE_H
#define LSMTREE_DISKTABLE_H

#include "../bloom_filter/BloomFilter.h"
#include "sstable/SSTable.h"
#include "../memtable/MemTable.h"
#include <map>
#include <list>
#include <algorithm>
#include <iterator>

class DiskTableNode {
protected:
    using Filter=BloomFilter<long long>;
    using IndexMap=std::map<long long, size_t>;

    SSTable *_sstable;
    Filter *filter;
    IndexMap *index;


    void loadIndexFilter();

public:
    DiskTableNode();

    DiskTableNode(DiskTableNode &&rhs) noexcept;

    explicit DiskTableNode(const path &p);

    ~DiskTableNode();

    bool mightIn(long long key);

    SSTableHeader *getHeader();

    IndexMap *getIndex();

    Filter *getFilter();

    SSTableData *getAllData();

    SSTableDataEntry getEntry(long long key);

    bool hasKey(long long key);

    bool intersect(DiskTableNode &rhs);

    bool intersect(SSTableData &rhs);

    bool valid(const SSTableDataEntry &s);

    void fillData(SSTableData &new_data);

    void clearDataCache();

    void fillData(SSTableData &&new_data);

    void writeToDisk(const path &&dst_file);

    void removeFromDisk();

    path getFile();
};


class DiskTable {
private:
    using DiskViewLevel=std::list<DiskTableNode>;
    using DiskView=std::vector<DiskViewLevel>;
    using DiskViewLevelIter=DiskView::iterator;
    using DataIter=SSTableData::iterator;
    using MergeSeq=std::pair<DataIter, DataIter>;
    using DiskNodeIter=DiskViewLevel::iterator;
    DiskView diskView;
    size_t SSTableClock;
    path db_home;

    template<typename ...Datas>
    SSTableData merge(Datas ...datas);

    const int LEVEL0_LIMIT = 2;
    const int LEVEL_FACTOR = 2;
    const int SSTABLE_SIZE_LIMIT = 2 * 1000 * 1000; // 2 MB(not MiB)

public:
    using QueryResult=struct {
        bool success;
        std::string data;
    };

    explicit DiskTable(path &db_dir);

    ~DiskTable();

    QueryResult get(long long int key);

    void persistent(MemTable &&m, bool df = false);
};

template<typename... Datas>
// Datas should be pointer to SSTableData(std::vector) object.
SSTableData DiskTable::merge(Datas... datas) {
    /* A MergeSeq is a pair abstracting a Sequence of SSTableDataEntry objects to be merged together.
     * It's made up of two iterator, from same SSTableData object, first one indicating current position of the sequence,
     * second one indicating the end of the sequence, so when first==second, the sequence has been consumed fully.
     */
    auto merge_seqs = std::vector<MergeSeq>{{datas->begin(), datas->end()}...};
    auto merge_result = SSTableData{};
    static auto compare_merge_seqs_prefer_current = [](const MergeSeq &candidate, const MergeSeq &current) {
        /*
         * Determining whether candidate or current has been fully consumed or not.
         * if candidate has ran out of and current not, take current, or vice versa.
         * if both candidate and current are fully consumed, continue to find another sequence.
         * When all sequences of merge_seqs have been fully consumed, min_element would
         * return a ended sequence eventually, and merge process terminates at that time.
         */
        if (candidate.first == candidate.second && current.first != current.second) {
            return false;
        }
        if (candidate.first != candidate.second && current.first == current.second) {
            return true;
        }
        if (candidate.first == candidate.second && current.first == current.second) {
            return false;
        }
        /*
         * If candidate and current have not been fully consumed, compare key of their
         * current entry, take smaller one, if their key equals, compare timestamp,
         * and take larger one.
         * If timestamp equals, takes current, because first in data takes precedence in merge function.
         */
        if (candidate.first->key < current.first->key) {
            return true;
        } else if (candidate.first->key == current.first->key) {
            return candidate.first->timestamp >
                   current.first->timestamp;
        } else {
            return false;
        }
    };
    while (true) {
        auto merge_resolution = *merge_seqs.begin();
        for (auto seq2 = merge_seqs.begin(); seq2 != merge_seqs.end(); seq2++) {
            if (compare_merge_seqs_prefer_current(*seq2, merge_resolution)) {
                merge_resolution = *seq2;
            }
        }
        std::for_each(merge_seqs.begin(), merge_seqs.end(), [merge_resolution](MergeSeq &seq) {
            if (seq.first->key == merge_resolution.first->key && seq.first != seq.second) {
                ++seq.first;
            }
        });
        if (merge_resolution.first == merge_resolution.second) {
            break;
        }
        merge_result.push_back(*merge_resolution.first);
    }
    return merge_result;
}


#endif //LSMTREE_DISKTABLE_H
