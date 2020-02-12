
#ifndef BPTREE_PUBLIC_H
#define BPTREE_PUBLIC_H
#include <cstdint>
namespace bptree{

    typedef int64_t ValueType;  // maybe an offset in a file
    typedef uint32_t KeyType;
    typedef uint64_t LocaType;
    const size_t DEGREE = 10;
    const size_t INTERNAL_MIN_ENTRY = (DEGREE-1)/2;
    const size_t LEAF_MIN_ENTRY = (DEGREE/2);
    const size_t INTERNAL_MAX_ENTRY = DEGREE-1;
    const size_t LEAF_MAX_ENTRY = DEGREE;
}
#endif //BPTREE_PUBLIC_H
