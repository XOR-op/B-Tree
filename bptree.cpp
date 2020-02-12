
#include <algorithm>
#include <cstring>
#include "debug.h"
#include "bptree.h"

using namespace bptree;
using std::tie;
using std::lower_bound;
using std::upper_bound;
using std::move;
using std::move_backward;


int BPTree::basic_search(const bptree::KeyType& key) {
    NodePtr cur = loadNode(root);
    path_stack[0] = cur;
    int counter = 0;
    while (cur->type == Node::INTERNAL) {
        int off = (key >= cur->K[0]) ? (int) (upper_bound(cur->K, cur->K+cur->size, key)-cur->K) : 0;
        cur = loadNode(cur->sub_nodes[off]);
        path_stack[++counter] = cur;
        in_node_offset_stack[counter] = off;
    }
    return counter;
}

std::pair<ValueType, bool> BPTree::search(const KeyType& key) {
    if (root==Node::NONE)
        return {0, false};
    NodePtr cur = path_stack[basic_search(key)];
    size_t i = lower_bound(cur->K, cur->K+cur->size, key)-cur->K;
    if (i < cur->size && key == cur->K[i])
        return {cur->V[i], true};
    else return {0, false};
}


size_t BPTree::insert_inplace(bptree::NodePtr& node, const bptree::KeyType& key, const bptree::ValueType& value) {
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

size_t BPTree::insert_key_inplace(bptree::NodePtr& node, const bptree::KeyType& key, LocaType offset) {
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

std::tuple<KeyType, LocaType>
BPTree::insert_key(bptree::NodePtr& cur, const bptree::KeyType& key, bptree::LocaType offset) {
    // @return new allocated node
    insert_key_inplace(cur, key, offset);
    NodePtr new_node = initNode(Node::INTERNAL);
    cur->size = DEGREE-INTERNAL_MIN_ENTRY-1;
    new_node->size = INTERNAL_MIN_ENTRY;
    move(cur->K+(DEGREE-INTERNAL_MIN_ENTRY), cur->K+DEGREE, new_node->K);
    move(cur->sub_nodes+(DEGREE-INTERNAL_MIN_ENTRY), cur->sub_nodes+DEGREE+1, new_node->sub_nodes);
    saveNode(cur);
    saveNode(new_node);
    // pass the deleted key back
    return {cur->K[cur->size], new_node->offset};
}

void BPTree::insert(const KeyType& key, const ValueType& value) {
    if (root==Node::NONE) {
        NodePtr ptr = initNode(Node::LEAF);
        ptr->prev=ptr->next=Node::NONE;
        insert_inplace(ptr, key, value);
        root=ptr->offset;
        return;
    }
    int cur_index = basic_search(key);
    if (path_stack[cur_index]->size < LEAF_MAX_ENTRY) {
        insert_inplace(path_stack[cur_index], key, value);
        return;
    }

    // split leaf node
    insert_inplace(path_stack[cur_index], key, value);
    NodePtr new_node = initNode(Node::LEAF);
    NodePtr& cur = path_stack[cur_index];
    // move and insert
    move(cur->K+DEGREE+1-LEAF_MIN_ENTRY, cur->K+DEGREE+1, new_node->K);
    move(cur->V+DEGREE+1-LEAF_MIN_ENTRY, cur->V+DEGREE+1, new_node->V);
    new_node->size = LEAF_MIN_ENTRY;
    cur->size = DEGREE+1-LEAF_MIN_ENTRY;
    new_node->prev = cur->offset;
    if (cur->next != Node::NONE) {
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
        NodePtr new_root = initNode(Node::INTERNAL);
        new_root->size = 1;
        new_root->K[0] = key_update_ready;
        new_root->sub_nodes[0] = path_stack[0]->offset;
        new_root->sub_nodes[1] = processing_offset;
        saveNode(new_root);
        root = new_root->offset;
    }
}


bool BPTree::remove_inplace(NodePtr& node, const KeyType& key) {
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

void BPTree::remove_offset_inplace(NodePtr& node, KeyType key, LocaType offset) {
    auto key_iter = std::lower_bound(node->K, node->K+node->size, key);
    move(key_iter+1, node->K+node->size, key_iter);

    auto off_iter = &node->sub_nodes[key_iter-node->K];
    if (*off_iter != offset)off_iter++;
    move(off_iter+1, node->sub_nodes+node->size+1, off_iter);
    --node->size;
    saveNode(node);
}

bool BPTree::borrow_value(int index) {
    NodePtr nearby = getLeft(index), node = path_stack[index];
    if (nearby && nearby->size > LEAF_MIN_ENTRY) {
        // Left
        move_backward(node->K, node->K+node->size, node->K+node->size+1);
        move_backward(node->V, node->V+node->size, node->V+node->size+1);
        find_mid_key(index, LEFT) = node->K[0] = nearby->K[nearby->size-1];
        node->V[0] = nearby->V[nearby->size-1];
    } else if ((nearby = getRight(index)) && nearby->size > LEAF_MIN_ENTRY) {
        // RIGHT
        node->K[node->size] = nearby->K[0];
        node->V[node->size] = nearby->V[0];
        move(nearby->K+1, nearby->K+nearby->size, nearby->K);
        move(nearby->V+1, nearby->V+nearby->size, nearby->V);
        find_mid_key(index, RIGHT) = nearby->K[0];
    } else return false;
    --nearby->size;
    ++node->size;
    saveNode(node);
    saveNode(nearby);
    return true;
}

bool BPTree::borrow_key(int index) {
    NodePtr nearby = getLeft(index), node = path_stack[index];
    if (nearby && nearby->size > LEAF_MIN_ENTRY) {
        // Left
        move_backward(node->K, node->K+node->size, node->K+node->size+1);
        move_backward(node->sub_nodes, node->sub_nodes+node->size+1, node->sub_nodes+node->size+2);
        node->K[0] = find_mid_key(index, LEFT);
        node->sub_nodes[0] = nearby->sub_nodes[nearby->size];
        find_mid_key(index, LEFT) = nearby->K[nearby->size-1];
    } else if ((nearby = getRight(index)) && nearby->size > LEAF_MIN_ENTRY) {
        // RIGHT
        node->K[node->size] = find_mid_key(index, RIGHT);
        node->sub_nodes[node->size+1] = nearby->sub_nodes[0];
        find_mid_key(index, RIGHT) = nearby->K[0];
        move(nearby->K+1, nearby->K+nearby->size, nearby->K);
        move(nearby->sub_nodes+1, nearby->sub_nodes+nearby->size+1, nearby->sub_nodes);
    } else return false;
    --nearby->size;
    ++node->size;
    saveNode(node);
    saveNode(nearby);
    return true;
}

LocaType BPTree::merge_values(NodePtr& target, NodePtr& tobe, int direction) {
    if (RIGHT == direction) {
        // tobe is left to target
        move_backward(target->K, target->K+target->size, target->K+target->size+tobe->size);
        move_backward(target->V, target->V+target->size, target->V+target->size+tobe->size);
        move(tobe->K, tobe->K+tobe->size, target->K);
        move(tobe->V, tobe->V+tobe->size, target->V);
        target->prev = tobe->prev;
        if (tobe->prev != Node::NONE) {
            NodePtr tobe_prev = loadNode(tobe->prev);
            tobe_prev->next = target->offset;
            saveNode(tobe_prev);
        }
    } else {
        // tobe is right to target
        move(tobe->K, tobe->K+tobe->size, target->K+target->size);
        move(tobe->V, tobe->V+tobe->size, target->V+target->size);
        target->next = tobe->next;
        if (tobe->next != Node::NONE) {
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

LocaType BPTree::merge_keys(KeyType mid_key, NodePtr& target, NodePtr& tobe, int direction) {
    if (RIGHT == direction) {
        move_backward(target->K, target->K+target->size, target->K+target->size+tobe->size+1);
        move_backward(target->sub_nodes, target->sub_nodes+target->size+1, target->sub_nodes+target->size+tobe->size+2);
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


bool BPTree::remove(const KeyType& key) {
    if (root==Node::NONE)
        return false;
    int cur_index = basic_search(key);
    if (!remove_inplace(path_stack[cur_index], key))return false;
    if (path_stack[cur_index]->size >= LEAF_MIN_ENTRY)return true;
    if (path_stack[0]->type == Node::LEAF) {
        // root case
        if (!path_stack[0]->size) {
            deleteNode(path_stack[0]);
            root = Node::NONE;
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

std::vector<std::pair<KeyType, ValueType>> BPTree::range(KeyType low, KeyType high) {
    decltype(range(0, 0)) ret;
    NodePtr ptr = path_stack[basic_search(low)];
    for (int i = (int) (lower_bound(ptr->K, ptr->K+ptr->size, low)-ptr->K); i < ptr->size; ++i) {
        if (ptr->K[i] >= high)goto FIN;
        ret.emplace_back(ptr->K[i], ptr->V[i]);
    }
    while (ptr->next != Node::NONE) {
        ptr = loadNode(ptr->next);
        for (int i = 0; i < ptr->size; ++i) {
            if (ptr->K[i] >= high)goto FIN;
            ret.emplace_back(ptr->K[i], ptr->V[i]);
        }
    }
    FIN:
    return ret;
}


void bptree::writeBuffer(Node* node, char* buf) {
# define write_attribute(ATTR) memcpy(buf,(void*)&node->ATTR,sizeof(node->ATTR));buf+=sizeof(node->ATTR)
    write_attribute(type);
    write_attribute(offset);
    write_attribute(next);
    if(node->type==Node::FREE)
        return;
    write_attribute(prev);
    write_attribute(size);
    memcpy(buf,(void*)&node->K, sizeof(KeyType)*DEGREE);
    buf+= sizeof(KeyType)*DEGREE;
    if(node->type==Node::LEAF)
        memcpy(buf,(void*)&node->V, sizeof(ValueType)*DEGREE);
    else
        memcpy(buf,(void*)&node->sub_nodes, sizeof(LocaType)*DEGREE);
#undef write_attribute
}

void bptree::readBuffer(Node* node, char* buf) {
#define read_attribute(ATTR) memcpy((void*)&node->ATTR,buf,sizeof(node->ATTR));buf+=sizeof(node->ATTR)
    read_attribute(type);
    read_attribute(offset);
    read_attribute(next);
    if(node->type==Node::FREE)
        return;
    read_attribute(prev);
    read_attribute(size);
    memcpy((void*)node->K,buf, sizeof(KeyType)*node->size);
    buf+= sizeof(KeyType)*DEGREE;
    if(node->type==Node::LEAF)
        memcpy((void*)node->V,buf, sizeof(ValueType)*node->size);
    else
        memcpy((void*)node->sub_nodes,buf, sizeof(LocaType)*(node->size+1));
#undef read_attribute
}
