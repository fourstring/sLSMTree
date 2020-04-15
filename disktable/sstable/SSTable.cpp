//
// Created by fourstring on 2020/4/4.
//

#include "SSTable.h"
#include <iostream>

template<>
long long int bytes_write<std::string>(std::ostream &os, const std::string *src, long long int count) {
    long long bytes = std::strlen(src->c_str());
    os << *src;
    return bytes;
}

template<>
long long int bytes_read<std::string>(std::istream &is, std::string *dst, long long int count) {
    //auto *buf = new char[count]{0};
    for (long long int i = 0; i < count; i++) {
        dst->push_back(is.get());
    }
    //*dst = std::string{buf};
    return count;
}

std::ifstream create_binary_ifstream(const path &file) {
    return std::ifstream(file, ios_base::in | ios_base::binary);
}

std::ofstream create_binary_ofstream(const path &file) {
    return std::ofstream(file, ios_base::out | ios_base::binary);
}

size_t size_of_entry(const SSTableDataEntry &s) {
    return sizeof(bool) + sizeof(time_t) + sizeof(long long) + sizeof(size_t) + s.value.length();
}

std::istream &operator>>(std::istream &is, SSTableDataEntry &s) {
    bytes_read(is, &s.delete_flag);
    bytes_read(is, &s.timestamp);
    bytes_read(is, &s.key);
    bytes_read(is, &s.value_length);
    bytes_read(is, &s.value, s.value_length);
    return is;
}

std::ofstream &operator<<(std::ofstream &os, const SSTableDataEntry &s) {
    bytes_write(os, &s.delete_flag);
    bytes_write(os, &s.timestamp);
    bytes_write(os, &s.key);
    bytes_write(os, &s.value_length);
    bytes_write(os, &s.value);
    return os;
}

bool SSTableDataEntry::operator<(const SSTableDataEntry &rhs) {
    return key < rhs.key;
}

SSTableDataEntry::SSTableDataEntry(bool d_f, time_t ts, long long k, std::string &&v) : delete_flag(d_f), timestamp(ts),
                                                                                        key(k), value(std::move(v)) {
    value_length = value.length();
}

SSTableDataEntry::SSTableDataEntry(bool d_f, time_t ts, long long k, const char *v) : delete_flag(d_f), timestamp(ts),
                                                                                      key(k), value(v) {
    value_length = value.length();
}


std::istream &operator>>(std::istream &is, SSTableHeader &s) {
    bytes_read(is, &s.index_offset);
    bytes_read(is, &s.entries_count);
    bytes_read(is, &s.key_min);
    bytes_read(is, &s.key_max);
    return is;
}

std::ofstream &operator<<(std::ofstream &os, const SSTableHeader &s) {
    bytes_write(os, &s.index_offset);
    bytes_write(os, &s.entries_count);
    bytes_write(os, &s.key_min);
    bytes_write(os, &s.key_max);
    return os;
}

std::istream &operator>>(std::istream &is, SSTableIndexItem &s) {
    bytes_read(is, &s.key);
    bytes_read(is, &s.offset);
    return is;
}

std::ofstream &operator<<(std::ofstream &os, const SSTableIndexItem &s) {
    bytes_write(os, &s.key);
    bytes_write(os, &s.offset);
    return os;
}


SSTableHeader *SSTable::getHeader() {
    if (header == nullptr) {
        auto in = create_binary_ifstream(file);
        header = new SSTableHeader{};
        in >> *header;
    }
    return header;
}

SSTableIndex *SSTable::getIndex() {
    if (index == nullptr) {
        auto *h = getHeader();
        auto in = create_binary_ifstream(file);
        in.seekg(h->index_offset);
        index = new SSTableIndex{};
        auto temp = SSTableIndexItem{};
        while (in >> temp) {
            index->push_back(temp);
        }
    }
    return index;
}

SSTableDataEntry SSTable::getEntry(long long offset) {
    // Just offer basic reading by offset,
    // search by key in index leave in DiskTable.
    auto in = create_binary_ifstream(file);
    auto temp = SSTableDataEntry{};
    in.seekg(offset);
    in >> temp;
    return temp;
}

SSTableData *SSTable::getAllData() {
    if (data == nullptr || data->empty()) {
        auto *h = getHeader();
        auto in = create_binary_ifstream(file);
        in.seekg(32); // Header always consume 32 bytes.
        data = new SSTableData{};
        auto temp = SSTableDataEntry{};
        while (in.tellg() < h->index_offset) {
            in >> temp;
            data->push_back(std::move(temp));
        }
    }
    return data;
}


void SSTable::fillData(SSTableData &new_data) {
    data = new SSTableData{new_data};
    long long key_min = 0, key_max = 0;
    size_t file_offset = 0, entries_count = data->size();
    // Determining header.
    key_min = data->begin()->key;
    key_max = data->rbegin()->key;
    file_offset = 32; // Header use 32 bytes.
    size_t item_bytes = 0;
    index = new SSTableIndex{};
    for (const auto &item:*data) {
        index->push_back({item.key, file_offset});
        item_bytes = size_of_entry(item);
        file_offset += item_bytes;
    }
    header = new SSTableHeader{file_offset, entries_count, key_min, key_max};
}

void SSTable::writeToDisk(const path &dst_file) {
    auto os = create_binary_ofstream(dst_file);
    os << *header;
    for (const auto &item:*data) {
        os << item;
    }
    for (const auto &item:*index) {
        os << item;
    }
    os.flush();
    os.close();
    file = dst_file; // Make connection between SSTable object and disk file.
}

SSTable::SSTable() = default; // default constructor for writing.

SSTable::SSTable(const path &filepath) {
    if (!exists(filepath)) {
        throw SSTableFileNotExistsException();
    }
    file = filepath;
}

SSTable::~SSTable() {
    delete header;
    delete data;
    delete index;
}

void SSTable::fillData(SSTableData &&new_data) {
    data = new SSTableData{std::move(new_data)};
    long long key_min = 0, key_max = 0;
    size_t file_offset = 0, entries_count = data->size();
    // Determining header.
    key_min = data->begin()->key;
    key_max = data->rbegin()->key;
    file_offset = 32; // Header use 32 bytes.
    size_t item_bytes = 0;
    index = new SSTableIndex{};
    for (const auto &item:*data) {
        index->push_back({item.key, file_offset});
        item_bytes = size_of_entry(item);
        file_offset += item_bytes;
    }
    header = new SSTableHeader{file_offset, entries_count, key_min, key_max};
}

void SSTable::clearDataCache() {
    // Just used to clear unnecessary data cached in SSTable object after merge, not to clear data stored in disk.
    delete data;
    data = nullptr;
}

void SSTable::removeFromDisk() {
    remove(file);
    delete data;
    delete header;
    delete index;
    data = nullptr;
    header = nullptr;
    index = nullptr;
}

SSTable::SSTable(SSTable &&rhs) noexcept {
    file = rhs.file;
    header = rhs.header;
    data = rhs.data;
    index = rhs.index;
    rhs.header = nullptr;
    rhs.data = nullptr;
    rhs.index = nullptr;
}


