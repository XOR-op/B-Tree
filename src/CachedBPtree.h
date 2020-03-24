
#ifndef BPTREE_CACHEDBPTREE_H
#define BPTREE_CACHEDBPTREE_H

#include <unordered_map>
#include <fstream>
#include <cstring>
#include <functional>
#include "bptree.h"

using std::ios;
namespace bptree {
    template <typename K,typename V>
    using func_expire_t =std::function<void(Node<K,V>*)>;
    template <typename K,typename V>
    using func_load_t=std::function<void(LocaType, Node<K,V>*)>;

    template <typename KeyType,typename ValueType>
    class LRUCache {
    private:
        /*
         * flag for double-linked list and freelist
         */
        typedef Node<KeyType,ValueType>* NodePtr;
        const static size_t LIST_END = 0;

        struct Block {
            size_t next;
            size_t prev;
            Node<KeyType,ValueType> node;

            Block() : next(LIST_END), prev(LIST_END) {}
        };

        size_t count;
        Block* pool;
        std::unordered_map<LocaType, size_t> table;

        size_t freelist_head;

        func_load_t<KeyType,ValueType> f_load;
        func_expire_t<KeyType,ValueType> f_expire;
    public:
        LRUCache(size_t block_count, func_load_t<KeyType,ValueType> load_func, func_expire_t<KeyType,ValueType> expire_func)
                : count(block_count), freelist_head(1), f_load(load_func), f_expire(expire_func) {
            pool = new Block[count+1];
            for (int i=1; i <count; ++i)
                pool[i].next = i+1;
            pool[count].next = LIST_END;
            pool[LIST_END].next=pool[LIST_END].prev=LIST_END;
        }

        LRUCache(const LRUCache&) = delete;

        LRUCache& operator=(const LRUCache&) = delete;

        bool remove(LocaType offset) {
            auto iter = table.find(offset);
            if (iter == table.end())return false;
            auto& block = pool[iter->second];
            pool[block.prev].next = block.next;
            pool[block.next].prev = block.prev;
            block.next = freelist_head;
            freelist_head = iter->second;
            f_expire(&block.node);
            table.erase(offset);
            return true;
        }

        NodePtr get(LocaType offset) {
            auto iter = table.find(offset);
            if (iter != table.end()) {
                // cache hit
                if (iter->second == pool[LIST_END].next) {
                    return &pool[pool[LIST_END].next].node;
                }
                auto& block = pool[iter->second];
                // remove from the ring
                pool[block.prev].next = block.next;
                pool[block.next].prev = block.prev;
                // insert
                block.next = pool[LIST_END].next;
                block.prev = LIST_END;
                pool[LIST_END].next = iter->second;
                pool[block.next].prev = iter->second;
                return &block.node;
            }
            // cache miss
            if (freelist_head == LIST_END)
                if(!remove(pool[pool[LIST_END].prev].node.offset))
                    throw std::runtime_error("???");
            auto tmp=pool[freelist_head].next;
            // set block the head
            pool[pool[LIST_END].next].prev = freelist_head;
            pool[freelist_head].prev = LIST_END;
            pool[freelist_head].next = pool[LIST_END].next;
            pool[LIST_END].next = freelist_head;
//            bzero(&pool[freelist_head].node, sizeof(pool[freelist_head].node));
            f_load(offset, &pool[freelist_head].node);
            table[offset]=freelist_head;
            freelist_head=tmp;
            return &pool[pool[LIST_END].next].node;
        }
        void destruct(){
            for (size_t index = pool[LIST_END].next; index != LIST_END; index = pool[index].next) {
                f_expire(&pool[index].node);
            }
            delete[] pool;
            pool= nullptr;
        }
        ~LRUCache() {
            // user must call destruct() manually.
        }
    };
    /*
     *  Cache size must be at least 4 times than the DEPTH in order to ensure work correctly.
     */

    template<typename KeyType, typename ValueType>
    class CachedBPTree;

    template<typename KeyType, typename ValueType>
    bool createTree(const std::string& path);

    template<typename KeyType, typename ValueType>
    class CachedBPTree : public BPTree<KeyType, ValueType> {
    private:
        static const size_t NO_FREE = SIZE_MAX;
        typedef Node<KeyType,ValueType>* NodePtr;

        LRUCache<KeyType,ValueType> cache;

        static void load(std::fstream& ifs, LocaType offset, NodePtr tobe_filled);

        static void flush(std::fstream& ofs, NodePtr node);

        NodePtr initNode(typename Node<KeyType, ValueType>::type_t t) override;

        void saveNode(NodePtr node) override;

        NodePtr loadNode(LocaType offset) override;

        void deleteNode(NodePtr node) override;

        std::fstream file;
        size_t file_size;
        LocaType freelist_head;
    public:
        explicit CachedBPTree(const std::string& path, size_t block_size);

        friend bool createTree<KeyType,ValueType>(const std::string& path);

        ~CachedBPTree();
    };



    template<typename KeyType,typename ValueType>
    void CachedBPTree<KeyType, ValueType>::flush(std::fstream& ofs, NodePtr node) {
        char buffer[Node<KeyType,ValueType>::BLOCK_SIZE];
        writeBuffer(node, buffer);
        ofs.seekp(node->offset);
        if (ofs.fail())throw std::runtime_error("CacheBPTree: Can't write");
        ofs.write(buffer, Node<KeyType,ValueType>::BLOCK_SIZE);
        ofs.flush();
        if (ofs.fail())throw std::runtime_error("CacheBPTree: Write failure");
    }

    template<typename KeyType,typename ValueType>
    void CachedBPTree<KeyType,ValueType>::load(std::fstream& ifs, bptree::LocaType offset, NodePtr tobe_filled) {
        char buffer[Node<KeyType,ValueType>::BLOCK_SIZE];
        ifs.seekg(offset);
        if (ifs.fail())throw std::runtime_error("CacheBPTree: Can't read");
        ifs.read(buffer, Node<KeyType,ValueType>::BLOCK_SIZE);
        readBuffer(tobe_filled, buffer);
        if (ifs.fail())throw std::runtime_error("CacheBPTree: Read failure");
    }

    template<typename KeyType,typename ValueType>
    Node<KeyType,ValueType>* CachedBPTree<KeyType,ValueType>::initNode(typename bptree::Node<KeyType,ValueType>::type_t t) {
        typedef Node<KeyType,ValueType> Node;
        if (freelist_head == NO_FREE) {
            // extend file
            char block[Node::BLOCK_SIZE];
            bzero(block, Node::BLOCK_SIZE);
            Node n;
            n.type = Node::FREE;
            n.offset = file_size;
            n.next = NO_FREE;
            writeBuffer(&n, block);
            file.seekp(file_size);
            file.write(block, Node::BLOCK_SIZE);
            file.flush();
            if (file.fail())throw std::runtime_error("CacheBPTree: initNode()");
            freelist_head = file_size;
            file_size += Node::BLOCK_SIZE;
        }
        NodePtr ptr = cache.get(freelist_head);
        freelist_head = ptr->next;
        ptr->type = t;
        return ptr;
    }

    template<typename KeyType,typename ValueType>
    void CachedBPTree<KeyType,ValueType>::saveNode(NodePtr node) {
        flush(file, node);
    }

    template<typename KeyType,typename ValueType>
    Node<KeyType,ValueType>* CachedBPTree<KeyType,ValueType>::loadNode(bptree::LocaType offset) {
        return cache.get(offset);
    }


    template<typename KeyType,typename ValueType>
    void CachedBPTree<KeyType,ValueType>::deleteNode(NodePtr node) {
        node->type = Node<KeyType,ValueType>::FREE;
        node->next = freelist_head;
        auto tmp = node->next;
        cache.remove(node->offset);
        freelist_head = tmp;
    }

    template <typename KeyType,typename ValueType>
    bool createTree(const std::string& path) {
        std::fstream f(path, ios::in | ios::out | ios::binary);
        if (f.is_open() || f.bad()) { return false; }
        f.close();
        f = std::fstream(path, ios::out | ios::binary);
#define write_attribute(ATTR) memcpy(ptr,(void*)&ATTR,sizeof(ATTR));ptr+=sizeof(ATTR)
        char buf[sizeof(CachedBPTree<KeyType,ValueType>::file_size)+sizeof(CachedBPTree<KeyType,ValueType>::freelist_head)+sizeof(CachedBPTree<KeyType,ValueType>::root)];
        char* ptr = buf;
        size_t size = sizeof(buf);
        LocaType free = CachedBPTree<KeyType,ValueType>::NO_FREE;
        LocaType t = Node<KeyType,ValueType>::NONE;
        write_attribute(size);
        write_attribute(free);
        write_attribute(t); // root
        write_attribute(t); // sequential_head
        f.write(buf, sizeof(buf));
        f.close();
        return true;
#undef write_attribute
    }

    template<typename KeyType,typename ValueType>
    CachedBPTree<KeyType,ValueType>::CachedBPTree(const std::string& path, size_t block_size) :
            BPTree<KeyType,ValueType>(), file(path, ios::in | ios::out | ios::binary),
            cache(block_size, [this](LocaType o, NodePtr r) { load(file, o, r); }, [this](NodePtr o) { flush(file, o); }) {
#define read_attribute(ATTR) memcpy((void*)&ATTR,ptr,sizeof(ATTR));ptr+=sizeof(ATTR)
        if (file.bad()) {
            throw std::runtime_error("CacheBPTree: Open B+ Tree failed.");
        }
        char buf[sizeof(file_size)+sizeof(freelist_head)+sizeof(this->root)];
        char* ptr = buf;
        file.seekg(0);
        file.read(buf, sizeof(buf));
        read_attribute(file_size);
        read_attribute(freelist_head);
        read_attribute(this->root);
#undef read_attribute
    }

    template<typename KeyType,typename ValueType>
    CachedBPTree<KeyType,ValueType>::~CachedBPTree() {
#define write_attribute(ATTR) memcpy(ptr,(void*)&ATTR,sizeof(ATTR));ptr+=sizeof(ATTR)
        char buf[sizeof(file_size)+sizeof(freelist_head)+sizeof(this->root)];
        char* ptr = buf;
        write_attribute(file_size);
        write_attribute(freelist_head);
        write_attribute(this->root);
        file.seekg(0);
        file.write(buf, sizeof(buf));
        cache.destruct();
        file.close();
#undef write_attribute
    }
}
#endif //BPTREE_CACHEDBPTREE_H
