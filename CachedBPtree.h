
#ifndef BPTREE_CACHEDBPTREE_H
#define BPTREE_CACHEDBPTREE_H

#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <cstring>
#include "bptree.h"
#include "Cache.h"

using std::ios;
namespace bptree {
    /*
     *  Cache size must be at least 4 times than the DEPTH in order to ensure work correctly.
     */
    class CachedBPTree : public BPTree {
    private:
        static const size_t NO_FREE = SIZE_MAX;

        LRUCache cache;

        static void load(std::fstream& ifs, LocaType offset, Node* tobe_filled);

        static void flush(std::fstream& ofs, Node* node);

        NodePtr initNode(Node::type_t t) override;

        void saveNode(NodePtr node) override;

        NodePtr loadNode(LocaType offset) override;

        void deleteNode(NodePtr node) override;

        std::fstream file;
        size_t file_size;
        LocaType freelist_head;
    public:
        explicit CachedBPTree(const std::string& path, size_t block_size);

        friend bool createTree(const std::string& path);

        ~CachedBPTree();
    };
    bool createTree(const std::string& path);

}
#endif //BPTREE_CACHEDBPTREE_H
