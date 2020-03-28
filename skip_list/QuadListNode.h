//
// Created by fourstring on 2020/3/6.
//

#ifndef SKIP_LIST_QUADLISTNODE_H
#define SKIP_LIST_QUADLISTNODE_H

template<typename T>
struct QuadListNode {
    T data;
    QuadListNode *pred, *succ, *above, *below;

    explicit QuadListNode(T data = T(), QuadListNode *pred = nullptr, QuadListNode *succ = nullptr,
                          QuadListNode *above = nullptr,
                          QuadListNode *below = nullptr);

    void insertAfterSucc(QuadListNode *new_node, QuadListNode *below_node = nullptr);

};

template<typename T>
QuadListNode<T>::QuadListNode(T data, QuadListNode *pred, QuadListNode *succ, QuadListNode *above, QuadListNode *below)
        :data(data), pred(pred), succ(succ), above(above), below(below) {}

template<typename T>
void QuadListNode<T>::insertAfterSucc(QuadListNode *new_node, QuadListNode *below_node) {
    auto *current_succ = this->succ;
    new_node->pred = this;
    new_node->succ = current_succ;

    current_succ->pred = new_node;
    this->succ = new_node;
    if (below_node) {
        // A node won't be inserted to the middle of a tower in a SkipList
        new_node->below = below_node;
        below_node->above = new_node;
    }
}


#endif //SKIP_LIST_QUADLISTNODE_H
