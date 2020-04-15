#include "kvstore.h"
#include <string>
#include <gzip/compress.hpp>
#include <gzip/decompress.hpp>
#include <gzip/utils.hpp>

KVStore::KVStore(const std::string &dir) : KVStoreAPI(dir) {
    auto data_dir = path(dir);
    lsmTree = new LSMTree{data_dir};
}

KVStore::~KVStore() {
    delete lsmTree;
}

/**
 * Insert/Update the key-value pair.
 * No return values for simplicity.
 */
void KVStore::put(uint64_t key, const std::string &s) {
    auto data = s;
    if (s.length() >= 100) {
        data = gzip::compress(s.data(), s.length());
    }
    lsmTree->put(key, data);
}

/**
 * Returns the (string) value of the given key.
 * An empty string indicates not found.
 */
std::string KVStore::get(uint64_t key) {
    auto compressed = lsmTree->get(key);
    if (gzip::is_compressed(compressed.data(), compressed.size())) {
        return gzip::decompress(compressed.data(), compressed.size());
    } else {
        return compressed;
    }
}

/**
 * Delete the given key-value pair if it exists.
 * Returns false iff the key is not found.
 */
bool KVStore::del(uint64_t key) {
    return lsmTree->del(key);
}

/**
 * This resets the kvstore. All key-value pairs should be removed,
 * including memtable and all sstables files.
 */
void KVStore::reset() {
    lsmTree->reset();
}
