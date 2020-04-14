//
// Created by fourstring on 2020/3/2.
//

#ifndef SKIP_LIST_DICTIONARY_H
#define SKIP_LIST_DICTIONARY_H

template<typename K, typename V>
class Dictionary {
public:
    [[nodiscard]] virtual long long size() const = 0;

    virtual bool put(K, V) = 0;

    virtual V *get(K k) = 0;

    virtual bool remove(K k) = 0;
};

template<typename K, typename V>
struct Entry {
    K key;
    V value;
};


#endif //SKIP_LIST_DICTIONARY_H
