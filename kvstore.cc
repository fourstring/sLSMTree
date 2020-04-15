#include "kvstore.h"
#include <string>
#include <gzip/compress.hpp>
#include <gzip/decompress.hpp>
#include <gzip/utils.hpp>
#include <csignal>
#include <atomic>

std::atomic<bool> gracefully_exit_flag = false;

KVStore::KVStore(const std::string &dir) : KVStoreAPI(dir) {
    auto data_dir = path(dir);
    lsmTree = new LSMTree{data_dir};
    std::signal(SIGINT, [](int sig) { gracefully_exit_flag.store(true); });
    std::signal(SIGTERM, [](int sig) { gracefully_exit_flag.store(true); });
}

KVStore::~KVStore() {
    delete lsmTree;
}

/**
 * Insert/Update the key-value pair.
 * No return values for simplicity.
 */
void KVStore::put(uint64_t key, const std::string &s) {
    check_gracefully_exit();
#ifdef WITH_GZIP
    auto data = s;
    if (s.length() >= 100) {
        data = gzip::compress(s.data(), s.length());
    }
    lsmTree->put(key, data);
#else
    lsmTree->put(key,s);
#endif
}

/**
 * Returns the (string) value of the given key.
 * An empty string indicates not found.
 */
std::string KVStore::get(uint64_t key) {
    check_gracefully_exit();
#ifdef WITH_GZIP
    auto compressed = lsmTree->get(key);
    if (gzip::is_compressed(compressed.data(), compressed.size())) {
        return gzip::decompress(compressed.data(), compressed.size());
    } else {
        return compressed;
    }
#else
    return lsmTree->get(key);
#endif
}

/**
 * Delete the given key-value pair if it exists.
 * Returns false iff the key is not found.
 */
bool KVStore::del(uint64_t key) {
    check_gracefully_exit();
    return lsmTree->del(key);
}

/**
 * This resets the kvstore. All key-value pairs should be removed,
 * including memtable and all sstables files.
 */
void KVStore::reset() {
    check_gracefully_exit();
    lsmTree->reset();
}

void KVStore::check_gracefully_exit() {
    if (gracefully_exit_flag.load()) {
        delete lsmTree;
        exit(0);
    }
}
