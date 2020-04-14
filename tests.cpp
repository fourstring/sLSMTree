//
// Created by fourstring on 2020/4/4.
//
#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <ctime>
#include "memtable/MemTable.h"
#include "disktable/DiskTable.h"

using namespace std::filesystem;

template<typename TestFunc, typename ...Args>
void it(const std::string &name, TestFunc f, Args... args) {
    std::cout << "It " << name << "...";
    if (f(args...)) {
        std::cout << " [Pass]";
    } else {
        std::cout << " [Failed]";
    }
    std::cout << std::endl;
}

bool test_memtable_move() {
    MemTable a{};
    MemTable b{std::move(a)};
    return b.levels() == 1 && a.levels() == 0;
}

bool test_string_fileio() {
    using std::ios_base;
    std::fstream testf;
    testf.open("test.bin", ios_base::in | ios_base::out | ios_base::binary | ios_base::trunc);
    std::string a{"abc"};
    std::string b{"def"};
    std::string c{};
    testf << a << ' ' << b;
    testf.close();
    testf.open("test.bin", ios_base::in | ios_base::out | ios_base::binary);
    testf >> c;
    bool res = c == "abc";
    remove("test.bin");
    return res;
}

bool test_memtable_behavior() {
    MemTable m{};
    m.put(1, "abcd");
    if (*m.get(1) != "abcd" && m.size_bytes() != 29) {
        return false;
    }

    m.put(1, "efgh");
    if (*m.get(1) != "efgh" && m.size_bytes() != 29) {
        return false;
    }

    if (m.get(2) != nullptr) {
        return false;
    }


    m.remove(1);

    if (m.get(1) != nullptr && m.size() != 1 && m.size_bytes() != 25) {
        return false;
    }

    m.put(1, "ijkl");

    if (*m.get(1) != "ijkl" && m.size_bytes() != 29) {
        return false;
    }

    m.remove(2);

    return m.size() == 2 && m.size_bytes() == 54;
}

bool test_bytes_level_IO() {
    using std::ios_base;
    std::fstream testf;
    testf.open("test.bin", ios_base::out | ios_base::binary);
    long long a = 12345;
    long long b = 6789;
    bytes_write(testf, &a);
    bytes_write(testf, &b);
    testf.close();
    testf.open("test.bin", ios_base::in | ios_base::binary);
    long long c = 0;
    long long d = 0;
    bytes_read(testf, &c);
    bytes_read(testf, &d);
    remove("test.bin");
    return !(c != a || d != b);
}

bool test_SSTable_behavior() {
    auto data = SSTableData{
            {false, 12345678,   1, "Hello,World!"},
            {false, 123455234,  2, "LLLLL"},
            {true,  1212212211, 3, ""},
            {false, 123312331,  4, "NIMO"}
    };
    auto s = SSTable{};
    s.fillData(data);

    auto *h = s.getHeader();

    if (h->key_min != 1 || h->entries_count != 4 || h->key_max != 4 || h->index_offset != 32 + (25 * 4 + 12 + 5 + 4)) {
        return false;
    }

    auto &index = *s.getIndex();

    if (
            index[0].key != 1 ||
            index[0].offset != 32 ||
            index[1].key != 2 ||
            index[1].offset != 69 ||
            index[2].key != 3 ||
            index[2].offset != 99 ||
            index[3].key != 4 ||
            index[3].offset != 124
            ) {
        return false;
    }

    return true;
}

bool test_SSTable_fileIO() {
    auto data = SSTableData{
            {false, 12345678,   1, "Hello,World!"},
            {false, 123455234,  2, "LLLLL"},
            {true,  1212212211, 3, ""},
            {false, 123312331,  4, "NIMO"}
    };
    auto t = SSTable{};
    t.fillData(data);
    t.writeToDisk("testf.bin");

    auto s = SSTable{"testf.bin"};

    auto *h = s.getHeader();

    if (h->key_min != 1 || h->entries_count != 4 || h->key_max != 4 || h->index_offset != 32 + (25 * 4 + 12 + 5 + 4)) {
        return false;
    }

    auto &index = *s.getIndex();

    if (
            index.size() != 4 ||
            index[0].key != 1 ||
            index[0].offset != 32 ||
            index[1].key != 2 ||
            index[1].offset != 69 ||
            index[2].key != 3 ||
            index[2].offset != 99 ||
            index[3].key != 4 ||
            index[3].offset != 124
            ) {
        return false;
    }
    auto &d = *s.getAllData();

    remove("testf.bin");
    return !(d.size() != 4 || d[3].delete_flag != false || d[3].timestamp != 123312331 || d[3].key != 4 ||
             d[3].value != "NIMO");
}

bool test_DiskTableNode_behavior() {
    auto data = SSTableData{
            {false, 12345678,   1,  "Hello,World!"},
            {false, 123455234,  2,  "LLLLL"},
            {true,  1212212211, 3,  ""},
            {false, 123312331,  4,  "NIMO"},
            {false, 1233123312, 10, "Hello,NIMO"}
    };
    auto data2 = SSTableData{
            {false, 12345678,   0,  "Hello,World!"},
            {false, 123455234,  2,  "LLLLL"},
            {true,  1212212211, 3,  ""},
            {false, 123312331,  4,  "NIMO"},
            {false, 1233123312, 15, "Hello,NIMO"}
    };
    auto data3 = SSTableData{
            {false, 123455234,  2, "LLLLL"},
            {true,  1212212211, 3, ""},
            {false, 123312331,  4, "NIMO"},
    };
    auto t = SSTable{};
    t.fillData(std::move(data));
    t.writeToDisk("testf.bin");

    auto node = DiskTableNode{path{"testf.bin"}};

    if (node.mightIn(11)) {
        return false;
    }

    if (!node.mightIn(10)) {
        return false;
    }
    std::cout << node.mightIn(5) << std::endl;

    auto e = node.getEntry(6);

    if (node.valid(e)) {
        return false;
    }

    e = node.getEntry(4);

    if (e.timestamp != 123312331 || e.value != "NIMO") {
        return false;
    }

    auto node2 = DiskTableNode{};
    auto node3 = DiskTableNode{};
    node2.fillData(std::move(data2));
    node3.fillData(std::move(data3));

    if (!node.intersect(node2) || !node.intersect(node3)) {
        return false;
    }

    node2.writeToDisk("testf2.bin");

    auto node4 = DiskTableNode{path{"testf2.bin"}};

    if (node4.getEntry(4).value != "NIMO") {
        return false;
    }

    remove("testf.bin");
    remove("testf2.bin");
    return true;
}

template<typename... Datas>
SSTableData merge(Datas... datas) {
    using DataIter=SSTableData::iterator;
    using MergeSeq=std::pair<DataIter, DataIter>;
    auto merge_seqs = std::vector<MergeSeq>{{datas->begin(), datas->end()}...};
    auto merge_result = SSTableData{};
    while (true) {
        auto merge_resolution = *std::min_element(merge_seqs.begin(), merge_seqs.end(),
                                                  [](const MergeSeq &seq1, const MergeSeq &seq2) {
                                                      if (seq1.first == seq1.second && seq2.first != seq2.second) {
                                                          return false;
                                                      }
                                                      if (seq1.first != seq1.second && seq2.first == seq2.second) {
                                                          return true;
                                                      }
                                                      if (seq1.first == seq1.second && seq2.first == seq2.second) {
                                                          return false;
                                                      }
                                                      if (seq1.first->key < seq2.first->key) {
                                                          return true;
                                                      } else if (seq1.first->key == seq2.first->key) {
                                                          return seq1.first->timestamp >=
                                                                 seq2.first->timestamp;
                                                      } else {
                                                          return false;
                                                      }
                                                  });
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

bool test_SSTableData_merge() {
    auto *data1 = new SSTableData{
            {false, 12345678,   1,  "Hello,World!"},
            {false, 123455234,  2,  "LLLLL"},
            {false, 1212212211, 3,  "asdasd"},
            {false, 123312331,  4,  "NIMO"},
            {false, 1233123312, 10, "Hello,NIMO"}
    };
    auto *data2 = new SSTableData{
            {false, 12345678,   0,  "Hello,World!"},
            {false, 123455235,  2,  "LLLLLL"},
            {true,  1212212212, 3,  ""},
            {false, 1233123,    4,  "NIMO!"},
            {false, 12331233,   15, "Hello,NIMO2"}
    };
    auto *data3 = new SSTableData{
            {false, 12345678,   20, "Hello,World!"},
            {false, 123455235,  22, "LLLLLL"},
            {true,  1212212212, 23, ""},
            {false, 1233123,    24, "NIMO!"},
            {false, 12331233,   25, "Hello,NIMO2"}
    };
    auto merged_data = SSTableData{merge(data1, data2, data3)};
    auto baseline = SSTableData{
            {false, 12345678,   0,  "Hello,World!"},
            {false, 12345678,   1,  "Hello,World!"},
            {false, 123455235,  2,  "LLLLLL"},
            {true,  1212212212, 3,  ""},
            {false, 123312331,  4,  "NIMO"},
            {false, 1233123312, 10, "Hello,NIMO"},
            {false, 12331233,   15, "Hello,NIMO2"},
            {false, 12345678,   20, "Hello,World!"},
            {false, 123455235,  22, "LLLLLL"},
            {true,  1212212212, 23, ""},
            {false, 1233123,    24, "NIMO!"},
            {false, 12331233,   25, "Hello,NIMO2"}
    };
    return std::equal(merged_data.begin(), merged_data.end(), baseline.begin(), baseline.end(),
                      [](const SSTableDataEntry &e1, const SSTableDataEntry &e2) {
                          return e1.delete_flag == e2.delete_flag && e1.timestamp == e2.timestamp &&
                                 e1.value_length == e2.value_length && e1.value_length == e2.value_length &&
                                 e1.value == e2.value;
                      });
}

bool test_vector_erase() {
    auto l = std::vector<int>{1, 2, 3, 4, 5};
    for (auto i = l.begin(); i != l.end();) {
        if (*i >= 3) {
            i = l.erase(i);
            continue;
        }
        i++;
    }
    auto base = std::list<int>{1, 2};
    return std::equal(l.begin(), l.end(), base.begin(), base.end());
}

bool test_SSTable_input() {
    auto l = std::list<std::list<int>>{{1, 2, 3, 4, 5},
                                       {6, 7, 8, 9, 10}};
    return true;
}

int main() {
    current_path("/home/fourstring/CLionProjects/lsmtree");
    it("should be able to move a memtable", test_memtable_move);
    it("should distinguish two strings by space in a binary file", test_string_fileio);
    it("should read/write number as bytes correctly", test_bytes_level_IO);
    it("should follow definition of MemTable", test_memtable_behavior);
    it("should correctly implement SSTable", test_SSTable_behavior);
    it("should correctly read/write bytes between SSTable in memory and disk.", test_SSTable_fileIO);
    it("should correctly implement DiskTableNode", test_DiskTableNode_behavior);
    it("should merge data correctly", test_SSTableData_merge);
    it("should correctly erase data in vector", test_vector_erase);
    it("should read sstable correctly", test_SSTable_input);
}