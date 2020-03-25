
#ifndef BPTREE_BPTREE_H
#define BPTREE_BPTREE_H

#include <memory>
#include <cstring>
#include "../include/algorithm.h"

using std::tie;
using ds::lower_bound;
using ds::upper_bound;
using ds::move;
using ds::move_backward;

namespace bptree {
    typedef uint64_t LocaType;
    const size_t DEGREE = 101;
    const size_t INTERNAL_MIN_ENTRY = (DEGREE-1)/2;
    const size_t LEAF_MIN_ENTRY = (DEGREE/2);
    const size_t INTERNAL_MAX_ENTRY = DEGREE-1;
    const size_t LEAF_MAX_ENTRY = DEGREE;
    /*
     *  KeyType needs  to be copyable without destructor, comparable and no duplication
     *  ValueType needs copyable without destructor
     */
    const size_t STACK_DEPTH = 20;


    template<typename KeyType, typename ValueType>
    struct Node {
        /*
         *  for internal node, max=DEGREE-1, min=floor((DEGREE-1)/2)
         *  for leaf node, max=DEGREE, min=floor(DEGREE/2)
         */
        const static LocaType NONE = SIZE_MAX;
        typedef enum {
            FREE, LEAF, INTERNAL
        } type_t;

        type_t type;
        LocaType offset; // when FREE, offset indicates what next free block is
        LocaType next;
        LocaType prev;
        size_t size;    // K.size
        KeyType K[DEGREE+1]; // DEGREE-1 if INTERNAL
        union {
            ValueType V[DEGREE+1];
            LocaType sub_nodes[DEGREE+1];  // more space for easier implementation of insert
        };


        const static size_t LEAF_SIZE = sizeof(type)+sizeof(offset)+sizeof(next)
                                        +sizeof(prev)+sizeof(size)+sizeof(KeyType)*DEGREE+sizeof(ValueType)*DEGREE;
        const static size_t INTERNAL_SIZE = sizeof(type)+sizeof(offset)+sizeof(next)
                                            +sizeof(prev)+sizeof(size)+sizeof(KeyType)*DEGREE+sizeof(LocaType)*DEGREE;
        const static size_t BLOCK_SIZE = std::max(LEAF_SIZE, INTERNAL_SIZE);
    };


    template<typename KeyType, typename ValueType>
    void writeBuffer(Node<KeyType, ValueType>* node, char* buf);

    template<typename KeyType, typename ValueType>
    void readBuffer(Node<KeyType, ValueType>* node, char* cuf);


    /*
     * first: index in parent's sub_nodes
     * second: pointer
     */

    template<typename KeyType, typename ValueType>
    class BPTree {
    private:
        enum {
            LEFT, RIGHT
        };
        static const int NO_PARENT = -1;

        typedef Node<KeyType, ValueType>* NodePtr;
        /*
         * data member
         */
        NodePtr path_stack[STACK_DEPTH];
        int in_node_offset_stack[STACK_DEPTH]{};


        int basic_search(const KeyType& key);

        size_t insert_inplace(NodePtr& node, const KeyType& key, const ValueType& value);
        size_t insert_key_inplace(NodePtr& node, const KeyType& key, LocaType offset);
        std::tuple<KeyType, LocaType> insert_key(NodePtr& node, const KeyType& key, LocaType offset);

        bool remove_inplace(NodePtr& node, const KeyType& key);
        void remove_offset_inplace(NodePtr& node, KeyType key, LocaType offset);
        bool borrow_key(int index);
        bool borrow_value(int index);
        LocaType merge_values(NodePtr& target, NodePtr& tobe, int direction);
        LocaType merge_keys(KeyType mid_key, NodePtr& target, NodePtr& tobe, int direction);

        KeyType& find_mid_key(int index, int direction) const {
            return (LEFT == direction) ? (path_stack[index-1]->K[in_node_offset_stack[index]-1]) :
                   (path_stack[index-1]->K[in_node_offset_stack[index]]);
        }


        NodePtr getLeft(int index) {
            return in_node_offset_stack[index] ? loadNode(path_stack[index-1]->sub_nodes[in_node_offset_stack[index]-1])
                                               : nullptr;
        }

        NodePtr getRight(int index) {
            return in_node_offset_stack[index] != path_stack[index-1]->size ? loadNode(
                    path_stack[index-1]->sub_nodes[in_node_offset_stack[index]+1]) : nullptr;
        }


    protected:
        virtual void saveNode(NodePtr node) = 0;;
        virtual void deleteNode(NodePtr node) = 0;
        virtual NodePtr loadNode(LocaType offset) = 0;
        virtual NodePtr initNode(typename Node<KeyType, ValueType>::type_t t) = 0;

        /*
         * maintain structure
         */
        LocaType root;
    public:
        BPTree() : root(Node<KeyType, ValueType>::NONE) {
            in_node_offset_stack[0] = NO_PARENT;
            for (auto& i : path_stack)i = nullptr;
        }

        /*
         *  search: if not found, pair.second = false
         *  @todo process same key
         */
        std::pair<ValueType, bool> search(const KeyType& key);

        void insert(const KeyType& key, const ValueType& value);

        bool remove(const KeyType& key);

        /*
         * range: low <= key < high
         */
        std::vector<std::pair<KeyType, ValueType>> range(KeyType low, KeyType high);

        ~BPTree() = default;
    };

    template<typename KeyType, typename ValueType>
    int BPTree<KeyType, ValueType>::basic_search(const KeyType& key) {
        NodePtr cur = loadNode(root);
        path_stack[0] = cur;
        int counter = 0;
        while (cur->type == Node<KeyType, ValueType>::INTERNAL) {
            int off = (key >= cur->K[0]) ? (int) (upper_bound(cur->K, cur->K+cur->size, key)-cur->K) : 0;
            cur = loadNode(cur->sub_nodes[off]);
            path_stack[++counter] = cur;
            in_node_offset_stack[counter] = off;
        }
        return counter;
    }

    template<typename KeyType, typename ValueType>
    std::pair<ValueType, bool> BPTree<KeyType, ValueType>::search(const KeyType& key) {
        if (root == Node<KeyType, ValueType>::NONE)
            return {0, false};
        NodePtr cur = path_stack[basic_search(key)];
        size_t i = lower_bound(cur->K, cur->K+cur->size, key)-cur->K;
        if (i < cur->size && key == cur->K[i])
            return {cur->V[i], true};
        else return {0, false};
    }


    template<typename KeyType, typename ValueType>
    size_t BPTree<KeyType, ValueType>::insert_inplace(NodePtr& node, const KeyType& key, const ValueType& value) {
        size_t i;
        if (!node->size) {
            node->K[0] = key;
            node->V[0] = value;
            i = 0;
        } else {
            auto& keys = node->K;
            i = upper_bound(keys, keys+node->size, key)-keys;
            move_backward(keys+i, keys+node->size, keys+node->size+1);
            move_backward(node->V+i, node->V+node->size, node->V+node->size+1);
            node->K[i] = key;
            node->V[i] = value;
        }
        ++node->size;
        saveNode(node);
        return i;
    }

    template<typename KeyType, typename ValueType>
    size_t BPTree<KeyType, ValueType>::insert_key_inplace(NodePtr& node, const KeyType& key, LocaType offset) {
        size_t i;
        if (!node->size) {
            node->K[0] = key;
            node->sub_nodes[0] = offset;
            i = 0;
        } else {
            auto& keys = node->K;
            i = upper_bound(keys, keys+node->size, key)-keys;
            move_backward(keys+i, keys+node->size, keys+node->size+1);
            move_backward(node->sub_nodes+i+1, node->sub_nodes+node->size+1, node->sub_nodes+node->size+2);
            node->K[i] = key;
            node->sub_nodes[i+1] = offset;
        }
        ++node->size;
        saveNode(node);
        return i;
    }

    template<typename KeyType, typename ValueType>
    std::tuple<KeyType, LocaType>
    BPTree<KeyType, ValueType>::insert_key(NodePtr& cur, const KeyType& key, bptree::LocaType offset) {
        // @return new allocated node
        insert_key_inplace(cur, key, offset);
        NodePtr new_node = initNode(Node<KeyType, ValueType>::INTERNAL);
        cur->size = DEGREE-INTERNAL_MIN_ENTRY-1;
        new_node->size = INTERNAL_MIN_ENTRY;
        move(cur->K+(DEGREE-INTERNAL_MIN_ENTRY), cur->K+DEGREE, new_node->K);
        move(cur->sub_nodes+(DEGREE-INTERNAL_MIN_ENTRY), cur->sub_nodes+DEGREE+1, new_node->sub_nodes);
        saveNode(cur);
        saveNode(new_node);
        // pass the deleted key back
        return {cur->K[cur->size], new_node->offset};
    }

    template<typename KeyType, typename ValueType>
    void BPTree<KeyType, ValueType>::insert(const KeyType& key, const ValueType& value) {
        if (root == Node<KeyType, ValueType>::NONE) {
            NodePtr ptr = initNode(Node<KeyType, ValueType>::LEAF);
            ptr->prev = ptr->next = Node<KeyType, ValueType>::NONE;
            insert_inplace(ptr, key, value);
            saveNode(ptr);
            root = ptr->offset;
            return;
        }
        int cur_index = basic_search(key);
        if (path_stack[cur_index]->size < LEAF_MAX_ENTRY) {
            insert_inplace(path_stack[cur_index], key, value);
            return;
        }

        // split leaf node
        insert_inplace(path_stack[cur_index], key, value);
        NodePtr new_node = initNode(Node<KeyType, ValueType>::LEAF);
        NodePtr& cur = path_stack[cur_index];
        // move and insert
        move(cur->K+DEGREE+1-LEAF_MIN_ENTRY, cur->K+DEGREE+1, new_node->K);
        move(cur->V+DEGREE+1-LEAF_MIN_ENTRY, cur->V+DEGREE+1, new_node->V);
        new_node->size = LEAF_MIN_ENTRY;
        cur->size = DEGREE+1-LEAF_MIN_ENTRY;
        new_node->prev = cur->offset;
        if (cur->next != Node<KeyType, ValueType>::NONE) {
            NodePtr c_next = loadNode(cur->next);
            new_node->next = c_next->offset;
            c_next->prev = new_node->offset;
            saveNode(c_next);
        }
        cur->next = new_node->offset;
        saveNode(new_node);
        saveNode(cur);

        // update parents
        --cur_index;
        KeyType key_update_ready = new_node->K[0];
        LocaType processing_offset = new_node->offset;
        bool set_root = true;
        for (; cur_index >= 0; --cur_index) {
            if (path_stack[cur_index]->size < INTERNAL_MAX_ENTRY) {
                insert_key_inplace(path_stack[cur_index], key_update_ready, processing_offset);
                set_root = false;
                break;
            } else {
                tie(key_update_ready, processing_offset) = insert_key(path_stack[cur_index], key_update_ready,
                                                                      processing_offset);
            }
        }
        if (set_root) {
            NodePtr new_root = initNode(Node<KeyType, ValueType>::INTERNAL);
            new_root->size = 1;
            new_root->K[0] = key_update_ready;
            new_root->sub_nodes[0] = path_stack[0]->offset;
            new_root->sub_nodes[1] = processing_offset;
            saveNode(new_root);
            root = new_root->offset;
        }
    }


    template<typename KeyType, typename ValueType>
    bool BPTree<KeyType, ValueType>::remove_inplace(NodePtr& node, const KeyType& key) {
        auto& vs = node->V;
        auto& ks = node->K;
        size_t i = std::lower_bound(ks, ks+node->size, key)-ks;
        if (ks[i] != key)
            return false;
        move(vs+i+1, vs+node->size, vs+i);
        move(ks+i+1, ks+node->size, ks+i);
        --node->size;
        saveNode(node);
        return true;
    }

    template<typename KeyType, typename ValueType>
    void BPTree<KeyType, ValueType>::remove_offset_inplace(NodePtr& node, KeyType key, LocaType offset) {
        auto key_iter = std::lower_bound(node->K, node->K+node->size, key);
        move(key_iter+1, node->K+node->size, key_iter);

        auto off_iter = &node->sub_nodes[key_iter-node->K];
        if (*off_iter != offset)off_iter++;
        move(off_iter+1, node->sub_nodes+node->size+1, off_iter);
        --node->size;
        saveNode(node);
    }

    template<typename KeyType, typename ValueType>
    bool BPTree<KeyType, ValueType>::borrow_value(int index) {
        NodePtr nearby = getLeft(index), node = path_stack[index];
        if (nearby && nearby->size > LEAF_MIN_ENTRY) {
            // Left
            move_backward(node->K, node->K+node->size, node->K+node->size+1);
            move_backward(node->V, node->V+node->size, node->V+node->size+1);
            find_mid_key(index, LEFT) = node->K[0] = nearby->K[nearby->size-1];
            node->V[0] = nearby->V[nearby->size-1];
            saveNode(path_stack[index-1]);
        } else if ((nearby = getRight(index)) && nearby->size > LEAF_MIN_ENTRY) {
            // RIGHT
            node->K[node->size] = nearby->K[0];
            node->V[node->size] = nearby->V[0];
            move(nearby->K+1, nearby->K+nearby->size, nearby->K);
            move(nearby->V+1, nearby->V+nearby->size, nearby->V);
            find_mid_key(index, RIGHT) = nearby->K[0];
            saveNode(path_stack[index-1]);
        } else return false;
        --nearby->size;
        ++node->size;
        saveNode(node);
        saveNode(nearby);
        return true;
    }

    template<typename KeyType, typename ValueType>
    bool BPTree<KeyType, ValueType>::borrow_key(int index) {
        NodePtr nearby = getLeft(index), node = path_stack[index];
        if (nearby && nearby->size > LEAF_MIN_ENTRY) {
            // Left
            move_backward(node->K, node->K+node->size, node->K+node->size+1);
            move_backward(node->sub_nodes, node->sub_nodes+node->size+1, node->sub_nodes+node->size+2);
            node->K[0] = find_mid_key(index, LEFT);
            node->sub_nodes[0] = nearby->sub_nodes[nearby->size];
            find_mid_key(index, LEFT) = nearby->K[nearby->size-1];
            saveNode(path_stack[index-1]);
        } else if ((nearby = getRight(index)) && nearby->size > LEAF_MIN_ENTRY) {
            // RIGHT
            node->K[node->size] = find_mid_key(index, RIGHT);
            node->sub_nodes[node->size+1] = nearby->sub_nodes[0];
            find_mid_key(index, RIGHT) = nearby->K[0];
            move(nearby->K+1, nearby->K+nearby->size, nearby->K);
            move(nearby->sub_nodes+1, nearby->sub_nodes+nearby->size+1, nearby->sub_nodes);
            saveNode(path_stack[index-1]);
        } else return false;
        --nearby->size;
        ++node->size;
        saveNode(node);
        saveNode(nearby);
        return true;
    }

    template<typename KeyType, typename ValueType>
    LocaType BPTree<KeyType, ValueType>::merge_values(NodePtr& target, NodePtr& tobe, int direction) {
        if (RIGHT == direction) {
            // tobe is left to target
            move_backward(target->K, target->K+target->size, target->K+target->size+tobe->size);
            move_backward(target->V, target->V+target->size, target->V+target->size+tobe->size);
            move(tobe->K, tobe->K+tobe->size, target->K);
            move(tobe->V, tobe->V+tobe->size, target->V);
            target->prev = tobe->prev;
            if (tobe->prev != Node<KeyType, ValueType>::NONE) {
                NodePtr tobe_prev = loadNode(tobe->prev);
                tobe_prev->next = target->offset;
                saveNode(tobe_prev);
            }
        } else {
            // tobe is right to target
            move(tobe->K, tobe->K+tobe->size, target->K+target->size);
            move(tobe->V, tobe->V+tobe->size, target->V+target->size);
            target->next = tobe->next;
            if (tobe->next != Node<KeyType, ValueType>::NONE) {
                NodePtr tobe_next = loadNode(tobe->next);
                tobe_next->prev = target->offset;
                saveNode(tobe_next);
            }
        }
        target->size += tobe->size;
        LocaType ret = tobe->offset;
        deleteNode(tobe);
        saveNode(target);
        return ret;
    }

    template<typename KeyType, typename ValueType>
    LocaType BPTree<KeyType, ValueType>::merge_keys(KeyType mid_key, NodePtr& target, NodePtr& tobe, int direction) {
        if (RIGHT == direction) {
            move_backward(target->K, target->K+target->size, target->K+target->size+tobe->size+1);
            move_backward(target->sub_nodes, target->sub_nodes+target->size+1,
                          target->sub_nodes+target->size+tobe->size+2);
            target->K[tobe->size] = mid_key;
            move(tobe->K, tobe->K+tobe->size, target->K);
            move(tobe->sub_nodes, tobe->sub_nodes+tobe->size+1, target->sub_nodes);
        } else {
            target->K[target->size] = mid_key;
            move(tobe->K, tobe->K+tobe->size, target->K+target->size+1);
            move(tobe->sub_nodes, tobe->sub_nodes+tobe->size+1, target->sub_nodes+target->size+1);
        }
        target->size += tobe->size+1;
        LocaType ret = tobe->offset;
        deleteNode(tobe);
        saveNode(target);
        return ret;
    }


    template<typename KeyType, typename ValueType>
    bool BPTree<KeyType, ValueType>::remove(const KeyType& key) {
        if (root == Node<KeyType, ValueType>::NONE)
            return false;
        int cur_index = basic_search(key);
        if (!remove_inplace(path_stack[cur_index], key))return false;
        if (path_stack[cur_index]->size >= LEAF_MIN_ENTRY)return true;
        if (path_stack[0]->type == Node<KeyType, ValueType>::LEAF) {
            // root case
            if (!path_stack[0]->size) {
                deleteNode(path_stack[0]);
                root = Node<KeyType, ValueType>::NONE;
            }
            return true;
        }
        NodePtr neighbor;
        // update these
        LocaType updating_offset;
        KeyType updating_key;
        if (borrow_value(cur_index)) {
            return true;
        } else if ((neighbor = getLeft(cur_index))) {
            updating_offset = merge_values(neighbor, path_stack[cur_index], LEFT);
            updating_key = find_mid_key(cur_index, LEFT);
        } else {
            neighbor = getRight(cur_index);
            updating_offset = merge_values(neighbor, path_stack[cur_index], RIGHT);
            updating_key = find_mid_key(cur_index, RIGHT);
        }
        for (--cur_index; cur_index; --cur_index) {
            remove_offset_inplace(path_stack[cur_index], updating_key, updating_offset);
            if (path_stack[cur_index]->size >= INTERNAL_MIN_ENTRY)return true;
            if (borrow_key(cur_index)) {
                return true;
            } else if ((neighbor = getLeft(cur_index))) {
                updating_key = find_mid_key(cur_index, LEFT);
                updating_offset = merge_keys(updating_key, neighbor, path_stack[cur_index], LEFT);
            } else {
                neighbor = getRight(cur_index);
                updating_key = find_mid_key(cur_index, RIGHT);
                updating_offset = merge_keys(updating_key, neighbor, path_stack[cur_index], RIGHT);
            }
        }
        remove_offset_inplace(path_stack[0], updating_key, updating_offset);
        if (!path_stack[0]->size) {
            NodePtr tmp = loadNode(path_stack[0]->sub_nodes[0]);
            deleteNode(path_stack[0]);
            root = tmp->offset;
        }
        return true;
    }

    template<typename KeyType, typename ValueType>
    std::vector<std::pair<KeyType, ValueType>> BPTree<KeyType, ValueType>::range(KeyType low, KeyType high) {
        decltype(range(KeyType(), KeyType())) ret;
        NodePtr ptr = path_stack[basic_search(low)];
        for (int i = (int) (lower_bound(ptr->K, ptr->K+ptr->size, low)-ptr->K); i < ptr->size; ++i) {
            if (ptr->K[i] >= high)goto FIN;
            ret.emplace_back(ptr->K[i], ptr->V[i]);
        }
        while (ptr->next != Node<KeyType, ValueType>::NONE) {
            ptr = loadNode(ptr->next);
            for (int i = 0; i < ptr->size; ++i) {
                if (ptr->K[i] >= high)goto FIN;
                ret.emplace_back(ptr->K[i], ptr->V[i]);
            }
        }
        FIN:
        return ret;
    }


    template<typename KeyType, typename ValueType>
    void writeBuffer(Node<KeyType, ValueType>* node, char* buf) {
# define write_attribute(ATTR) memcpy(buf,(void*)&node->ATTR,sizeof(node->ATTR));buf+=sizeof(node->ATTR)
        write_attribute(type);
        write_attribute(offset);
        write_attribute(next);
        if (node->type == Node<KeyType, ValueType>::FREE)
            return;
        write_attribute(prev);
        write_attribute(size);
        memcpy(buf, (void*) &node->K, sizeof(KeyType)*DEGREE);
        buf += sizeof(KeyType)*DEGREE;
        if (node->type == Node<KeyType, ValueType>::LEAF)
            memcpy(buf, (void*) &node->V, sizeof(ValueType)*DEGREE);
        else
            memcpy(buf, (void*) &node->sub_nodes, sizeof(LocaType)*DEGREE);
#undef write_attribute
    }

    template<typename KeyType, typename ValueType>
    void readBuffer(Node<KeyType, ValueType>* node, char* buf) {
#define read_attribute(ATTR) memcpy((void*)&node->ATTR,buf,sizeof(node->ATTR));buf+=sizeof(node->ATTR)
        read_attribute(type);
        read_attribute(offset);
        read_attribute(next);
        if (node->type == Node<KeyType, ValueType>::FREE)
            return;
        read_attribute(prev);
        read_attribute(size);
        memcpy((void*) node->K, buf, sizeof(KeyType)*node->size);
        buf += sizeof(KeyType)*DEGREE;
        if (node->type == Node<KeyType, ValueType>::LEAF)
            memcpy((void*) node->V, buf, sizeof(ValueType)*node->size);
        else
            memcpy((void*) node->sub_nodes, buf, sizeof(LocaType)*(node->size+1));
#undef read_attribute
    }

}
#endif //BPTREE_BPTREE_H
