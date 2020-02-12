
#ifndef BPTREE_BPTREE_H
#define BPTREE_BPTREE_H

#include <memory>
#include <vector>
#include "public.h"

namespace bptree {
    /*
     *  KeyType needs  to be copyable without destructor, comparable and no duplication
     *  ValueType needs copyable without destructor
     */
//    typedef uint32_t size_t;
    const size_t STACK_DEPTH = 20;

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

    void writeBuffer(Node* node, char* buf);

    void readBuffer(Node* node, char* cuf);

    typedef Node* NodePtr;

    /*
     * first: index in parent's sub_nodes
     * second: pointer
     */


    class BPTree {
    private:
        enum {
            LEFT, RIGHT
        };
        static const int NO_PARENT = -1;

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

        virtual NodePtr initNode(Node::type_t t) = 0;

        /*
         * maintain structure
         */
        LocaType root;
        // todo bug remained
    public:
        BPTree() : root(Node::NONE) {
            in_node_offset_stack[0] = NO_PARENT;
            for (auto& i : path_stack)i = nullptr;
        }

        /*
         *  search: if not found, pair.second = false
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
}
#endif //BPTREE_BPTREE_H
