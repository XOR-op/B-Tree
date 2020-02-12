
#ifndef BPTREE_CACHE_H
#define BPTREE_CACHE_H

#include <unordered_map>
#include <functional>
#include "debug.h"
#include "bptree.h"

namespace bptree {

    using func_expire_t=std::function<void(Node*)>;
    using func_load_t=std::function<void(LocaType, Node*)>;

    class LRUCache {
    private:
        /*
         * flag for double-linked list and freelist
         */
        const static size_t LIST_END = 0;

        struct Block {
            size_t next;
            size_t prev;
            Node node;

            Block() : next(LIST_END), prev(LIST_END) {}
        };

        size_t count;
        Block* pool;
        std::unordered_map<LocaType, size_t> table;

        size_t freelist_head;

        func_load_t f_load;
        func_expire_t f_expire;
    public:
        LRUCache(size_t block_count, func_load_t load_func, func_expire_t expire_func)
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

        Node* get(LocaType offset) {
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
}
#endif //BPTREE_CACHE_H
