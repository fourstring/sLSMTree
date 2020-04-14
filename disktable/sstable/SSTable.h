//
// Created by fourstring on 2020/4/4.
//

#ifndef LSMTREE_SSTABLE_H
#define LSMTREE_SSTABLE_H

#include <string>
#include <fstream>
#include <vector>
#include <filesystem>
#include <exception>
#include <cstring>
#include <algorithm>

using namespace std::filesystem;
using std::ios_base;

std::ifstream create_binary_ifstream(const path &file);

std::ofstream create_binary_ofstream(const path &file);

template<typename T>
long long int bytes_read(std::istream &is, T *dst, long long int count = 0) {
    long long bytes = count != 0 ? count : sizeof(T);
    is.read(reinterpret_cast<char *>(dst), bytes);
    return bytes;
}

template<typename T>
long long int bytes_write(std::ostream &os, const T *src, long long int count = 0) {
    long long bytes = count != 0 ? count : sizeof(T);
    os.write(reinterpret_cast<const char *>(src), bytes);
    return bytes;
}

struct SSTableHeader {
    size_t index_offset;
    size_t entries_count;
    long long key_min;
    long long key_max;

    friend std::istream &operator>>(std::istream &is, SSTableHeader &s);

    friend std::ofstream &operator<<(std::ofstream &os, const SSTableHeader &s);
};

// Delim of two entries is ' '
struct SSTableDataEntry {
    bool delete_flag = false;
    time_t timestamp;
    long long key;
    size_t value_length;
    std::string value;

    SSTableDataEntry() = default;

    SSTableDataEntry(bool d_f, time_t ts, long long k, std::string &&v);

    SSTableDataEntry(bool d_f, time_t ts, long long k, const char *v);

    friend std::istream &operator>>(std::istream &is, SSTableDataEntry &s);

    friend std::ofstream &operator<<(std::ofstream &os, const SSTableDataEntry &s);

    bool operator<(const SSTableDataEntry &rhs);

};


size_t size_of_entry(const SSTableDataEntry &s);

using SSTableData=std::vector<SSTableDataEntry>;

struct SSTableIndexItem {
    long long key;
    size_t offset;

    friend std::istream &operator>>(std::istream &is, SSTableIndexItem &s);

    friend std::ofstream &operator<<(std::ofstream &os, const SSTableIndexItem &s);
};

using SSTableIndex=std::vector<SSTableIndexItem>;

class SSTableFileNotExistsException : public std::exception {
};

class SSTable {
private:
    path file;
    SSTableHeader *header{};
    SSTableData *data{};
    SSTableIndex *index{};
public:

    explicit SSTable();

    SSTable(SSTable &&rhs) noexcept;

    explicit SSTable(const path &filepath);

    SSTableHeader *getHeader();

    SSTableDataEntry getEntry(long long key);

    SSTableData *getAllData();

    SSTableIndex *getIndex();

    ~SSTable();

    void fillData(SSTableData &new_data);

    void fillData(SSTableData &&new_data);

    void clearDataCache();

    void writeToDisk(const path &dst_file);

    void removeFromDisk();
};


#endif //LSMTREE_SSTABLE_H
