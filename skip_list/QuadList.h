//
// Created by fourstring on 2020/3/2.
//

#ifndef SKIP_LIST_QUADLIST_H
#define SKIP_LIST_QUADLIST_H

#include "QuadListNode.h"

template<typename T>
class QuadList {
public:
    using Node=QuadListNode<T>;
    using NodePosi=QuadListNode<T> *;
private:
    NodePosi header, trailer;
    int _size = 0;
public:

    const NodePosi &first() { return header; };

    const NodePosi &last() { return trailer; };

    QuadList();

    QuadList(QuadList &&q) noexcept;

    virtual ~QuadList();

    void init();

    void clear();

    int empty() { return _size == 0; }

    int size() { return _size; }

    auto insertAfterAbove(T data, NodePosi pred_node, NodePosi below_node = nullptr);

    void remove(NodePosi to_delete);

    bool valid(NodePosi p) {
        return p && p != header && p != trailer;
    }

};

template<typename T>
QuadList<T>::QuadList() {
    init();
}

template<typename T>
QuadList<T>::~QuadList() {
    if (!empty())
        clear();
    delete header;
    delete trailer;
}

template<typename T>
void QuadList<T>::init() {
    header = new Node();
    trailer = new Node();
    header->succ = trailer;
    trailer->pred = header;
}

template<typename T>
void QuadList<T>::clear() {
    auto *current = header->succ;
    while (current != trailer) {
        auto *next = current->succ;
        delete current;
        current = next;
    }
}

template<typename T>
auto QuadList<T>::insertAfterAbove(T data, NodePosi pred_node, NodePosi below_node) {
    auto *new_node = new Node(data);
    pred_node->insertAfterSucc(new_node, below_node);
    _size++;
    return const_cast<const NodePosi &>(new_node);
}

template<typename T>
void QuadList<T>::remove(NodePosi to_delete) {
    to_delete->pred->succ = to_delete->succ;
    to_delete->succ->pred = to_delete->pred;
    if (to_delete->below)
        to_delete->below->above = nullptr;
    if (to_delete->above)
        to_delete->above->below = nullptr;
    delete to_delete;
    _size--;
}

template<typename T>
QuadList<T>::QuadList(QuadList &&q) noexcept {
    this->header = q.header;
    this->trailer = q.trailer;
    this->_size = q._size;
    q.header = nullptr;
    q.trailer = nullptr;
}


#endif //SKIP_LIST_QUADLIST_H
