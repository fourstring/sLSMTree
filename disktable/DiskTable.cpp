//
// Created by fourstring on 2020/4/4.
//

#include "DiskTable.h"

DiskTableNode::DiskTableNode() : _sstable{nullptr}, filter{nullptr}, index{nullptr} {
} // For writing.

DiskTableNode::DiskTableNode(const path &p) : _sstable{new SSTable{p}}, filter{nullptr}, index{nullptr} {
}

DiskTableNode::~DiskTableNode() {
    delete _sstable;
    delete filter;
    delete index;
}

SSTableHeader *DiskTableNode::getHeader() {
    return _sstable->getHeader();
}

void DiskTableNode::loadIndexFilter() {
    if (index == nullptr && filter == nullptr) {
        // Info from _sstable->index is used to generate filter and index, dont need after that.
        auto ssindex = SSTableIndex{std::move(*_sstable->getIndex())};
        index = new IndexMap{};
        auto count = getHeader()->entries_count;
        filter = new Filter{10 * count, count, 0};
        for (const auto &item:ssindex) {
            index->insert({item.key, item.offset});
            filter->add(item.key);
        }
    }
}

DiskTableNode::IndexMap *DiskTableNode::getIndex() {
    if (index == nullptr || index->empty() || index->size() == 0) {
        loadIndexFilter();
    }
    return index;
}

DiskTableNode::Filter *DiskTableNode::getFilter() {
    if (filter == nullptr) {
        loadIndexFilter();
    }
    return filter;
}

SSTableDataEntry DiskTableNode::getEntry(long long key) {
    auto *i = getIndex();
    auto p = i->find(key);
    if (p == i->end()) {
        return SSTableDataEntry{false, 0, 0, ""};
    }
    auto offset = p->second;
    return _sstable->getEntry(offset);
}

bool DiskTableNode::mightIn(long long key) {
    auto *h = getHeader();
    auto *f = getFilter();
    return key >= h->key_min && key <= h->key_max && f->find(key);
}

bool DiskTableNode::intersect(DiskTableNode &rhs) {
    auto *h = getHeader();
    auto *rh = rhs.getHeader();
    return ((rh->key_min >= h->key_min && rh->key_min <= h->key_max) ||
            (rh->key_max >= h->key_min && rh->key_max <= h->key_max)) ||
           (h->key_min >= rh->key_min && h->key_max <= rh->key_max);
}

bool DiskTableNode::hasKey(long long key) {
    auto *i = getIndex();
    return mightIn(key) && index->find(key) != i->end();
}

bool DiskTableNode::valid(const SSTableDataEntry &s) {
    return s.timestamp >
           0; // getEntry return an entry whose timestamp is 0 to indicate that there is no key in the sstable.
    /* We can not add check of delete_flag of s in this method, which is used to indicate whether found given
     key in the DiskNode, though the key is marked deleted in corresponding sstable, it's do in that sstable,
     and expected behavior is terminating search in DiskTable, not continue to search other sstable like there is
     no given key in the sstable.*/
}

void DiskTableNode::fillData(SSTableData &new_data) {
    _sstable = new SSTable{};
    _sstable->fillData(new_data);
}

void DiskTableNode::writeToDisk(const path &&dst_file) {
    if (_sstable != nullptr) {
        _sstable->writeToDisk(dst_file);
    }
}

SSTableData *DiskTableNode::getAllData() {
    if (_sstable != nullptr) {
        return _sstable->getAllData();
    }
    return nullptr;
}

void DiskTableNode::fillData(SSTableData &&new_data) {
    _sstable = new SSTable{};
    _sstable->fillData(std::forward<SSTableData>(new_data));
}

void DiskTableNode::clearDataCache() {
    _sstable->clearDataCache();
}

void DiskTableNode::removeFromDisk() {
    _sstable->removeFromDisk();
    delete _sstable;
    delete index;
    delete filter;
    _sstable = nullptr;
    index = nullptr;
    filter = nullptr;
}

bool DiskTableNode::intersect(SSTableData &rhs) {
    auto *h = getHeader();
    auto rkey_min = rhs.begin()->key;
    auto rkey_max = rhs.rbegin()->key;
    return ((rkey_min >= h->key_min && rkey_min <= h->key_max) ||
            (rkey_max >= h->key_min && rkey_max <= h->key_max)) ||
           (h->key_min >= rkey_min && h->key_max <= rkey_max);
}

DiskTableNode::DiskTableNode(DiskTableNode &&rhs) noexcept {
    _sstable = rhs._sstable;
    filter = rhs.filter;
    index = rhs.index;
    rhs._sstable = nullptr;
    rhs.filter = nullptr;
    rhs.index = nullptr;
}

DiskTable::QueryResult DiskTable::get(long long int key) {
    auto cur_level = diskView.begin();
    auto level_end = diskView.end();
    for (; cur_level != level_end; cur_level++) {
        if (cur_level == diskView.begin()) {
            // To ensure correctness of result, we should lookup one written to disk later first in level 0 (if there are two sstable already).
            auto cur_node = cur_level->rbegin();
            auto rend = cur_level->rend();
            for (; cur_node != rend; cur_node++) {
                if (cur_node->mightIn(key)) {
                    auto res = cur_node->getEntry(key);
                    if (cur_node->valid(res)) {
                        if (res.delete_flag) {
                            // Delete record must leave in disk.
                            // See also comment in DiskTableNode#valid.
                            return {false, ""};
                        }
                        return {true, std::move(res.value)};
                    }
                }
            }
        } else {
            auto cur_node = cur_level->begin();
            auto end = cur_level->end();
            for (; cur_node != end; cur_node++) {
                if (cur_node->mightIn(key)) {
                    auto res = cur_node->getEntry(key);
                    if (cur_node->valid(res)) {
                        if (res.delete_flag) {
                            return {false, ""};
                        }
                        return {true, std::move(res.value)};
                    }
                }
            }
        }
    }
    return {false, ""};
}

void DiskTable::persistent(MemTable &&m, bool df) {
    auto new_disk_node = DiskTableNode{};
    new_disk_node.fillData(m.collectData());
    auto level0 = diskView.begin();
    auto writeFileName = [this](DiskViewLevelIter level) {
        this->SSTableClock += 1;
        auto parent_dir = this->db_home / path{std::to_string(std::distance(this->diskView.begin(), level))};
        if (!exists(parent_dir)) {
            create_directory(parent_dir);
        }
        return parent_dir / (path{std::to_string(this->SSTableClock) + ".bin"});
    };
    if (level0->size() < LEVEL0_LIMIT) {
        new_disk_node.writeToDisk(writeFileName(level0));
        new_disk_node.clearDataCache();
        level0->push_back(std::move(new_disk_node));
        return;
    }
    if (level0->size() == LEVEL0_LIMIT) {
        auto cur_level = diskView.begin();
        if (std::next(cur_level) == diskView.end()) {
            diskView.push_back(DiskViewLevel{});
        }
        auto compaction_into_level = std::next(cur_level);
        auto compaction_into_level_limit = LEVEL0_LIMIT * LEVEL_FACTOR;
        auto overflow_data = SSTableData{};
        while (true) {
            if (cur_level == diskView.begin()) {
                auto level0_0 = cur_level->begin();
                auto level0_1 = std::next(level0_0, 1);
                overflow_data = merge(new_disk_node.getAllData(), level0_1->getAllData(), level0_0->getAllData());
                level0_0->removeFromDisk();
                level0_1->removeFromDisk();
                level0->clear();
            }

            for (auto to_compaction_node = compaction_into_level->begin();
                 to_compaction_node != compaction_into_level->end();) {
                if (to_compaction_node->intersect(overflow_data)) {
                    overflow_data = merge(&overflow_data, to_compaction_node->getAllData());
                    to_compaction_node->clearDataCache();
                    to_compaction_node->removeFromDisk();
                    to_compaction_node = compaction_into_level->erase(to_compaction_node);
                    continue;
                }
                to_compaction_node++;
            }

            while (compaction_into_level->size() < compaction_into_level_limit && !overflow_data.empty()) {
                auto persistent_data_block = SSTableData{};
                volatile size_t blocked_data_bytes = 0;
                while (blocked_data_bytes <= SSTABLE_SIZE_LIMIT && !overflow_data.empty()) {
                    blocked_data_bytes += size_of_entry(*overflow_data.begin());
                    persistent_data_block.push_back(*overflow_data.begin());
                    overflow_data.erase(overflow_data.begin());
                }
                auto persistent_node = DiskTableNode{};
                persistent_node.fillData(std::move(persistent_data_block));
                persistent_node.writeToDisk(writeFileName(compaction_into_level));
                persistent_node.clearDataCache();
                compaction_into_level->push_back(std::move(persistent_node));
            }

            if (overflow_data.empty()) {
                break;
            }

            if (std::next(compaction_into_level) == diskView.end()) {
                diskView.push_back(DiskViewLevel{});
            }

            cur_level++;
            compaction_into_level++;
            compaction_into_level_limit *= LEVEL_FACTOR;
        }
    }
}

DiskTable::DiskTable(path &db_dir) {
    /*
     * Scan db_dir, build diskView.
     * Structure of db_dir like this:
         db_dir
              | <dir> 0
              | <dir> 1
              | <dir> ...
              | sstable.lock // store SSTableClock, for simplifying naming of SSTable.
     * Every sub dir corresponding to a level according to its name(level number)
     * In every sub dir, there are some SSTable, whose filename is just SSTableClock when it was written to disk,
     * such naming is for convenience of relocating SSTable in a level when compaction.
    */
    db_home = db_dir;
    if (!exists(db_dir)) {
        create_directory(db_dir);
    }
    if (!exists(db_dir / "sstable.lock")) {
        SSTableClock = 0;
    } else {
        auto clock_is = create_binary_ifstream(db_dir / "sstable.lock");
        bytes_read(clock_is, &SSTableClock);
    }

    diskView = DiskView{};
    diskView.reserve(64);
    auto dir = directory_iterator{db_dir};
    auto dir_buf = std::vector<directory_entry>{}; // directory_iterator is out-of-order, use a buf to sort them.
    for (const auto &d:dir) {
        if (d.is_directory()) {
            // Skip other files in db_dir.
            dir_buf.push_back(d);
        }
    }
    if (dir_buf.empty()) {
        create_directory(db_dir / "0");
        dir_buf.emplace_back(db_dir / "0");
    }
    std::stable_sort(dir_buf.begin(), dir_buf.end());
    for (const auto &level:dir_buf) {
        auto new_view_level = DiskViewLevel{};
        auto node_path_buf = std::vector<path>{};
        for (const auto &sstable_file:directory_iterator{level.path()}) {
            node_path_buf.push_back(sstable_file.path());
        }
        // To ensure correctness, we must enforce that sstable written later in level 0 placed into diskView[0][1](if exists)
        std::stable_sort(node_path_buf.begin(), node_path_buf.end());
        for (const auto &node_path:node_path_buf) {
            new_view_level.emplace_back(node_path);
        }
        diskView.push_back(std::move(new_view_level));
    }
}

DiskTable::~DiskTable() {
    auto clock_os = create_binary_ofstream(db_home / "sstable.lock");
    bytes_write(clock_os, &SSTableClock); // Sync SSTableClock to disk when exit.
}







