# B-Tree
A single-file high performance B+ Tree, with LRU cache in memory to boost the performance.
## Use
bptree::CachedBPTree has integrated LRU cache in it. You can include `CachedBPTree.h` to use it.
- search(K): search specified key and return std::pair<KeyType,bool>. Not found if `pair->second` is False.
- insert(K, V): insert a pair of data
- remove(K): remove the pair with the specified key
- range(K_low, K_high): get a range of data subject to K_low <= key < K_high
