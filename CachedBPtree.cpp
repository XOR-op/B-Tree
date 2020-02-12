
#include <strings.h>
#include "CachedBPtree.h"
#include "public.h"
#include "debug.h"

using namespace bptree;

void bptree::CachedBPTree::flush(std::fstream& ofs, Node* node) {
    //
    char buffer[Node::BLOCK_SIZE];
//    bzero(buffer, Node::BLOCK_SIZE);
    writeBuffer(node, buffer);
    ofs.seekp(node->offset);
    FUNC_CALL(node->offset);
    if (ofs.fail())throw std::runtime_error("Can't write");
    ofs.write(buffer, Node::BLOCK_SIZE);
    ofs.flush();
    if (ofs.fail())throw std::runtime_error("Write failure");
}

void bptree::CachedBPTree::load(std::fstream& ifs, bptree::LocaType offset, bptree::Node* tobe_filled) {
    if(offset==0){
        int i=1;
    }
    char buffer[Node::BLOCK_SIZE];
//    bzero(buffer, Node::BLOCK_SIZE);
    ifs.seekg(offset);
    FUNC_CALL(offset);
    if (ifs.fail())throw std::runtime_error("Can't read");
    ifs.read(buffer, Node::BLOCK_SIZE);
    readBuffer(tobe_filled, buffer);
    if(ifs.fail())throw std::runtime_error("Read failure");
}

bptree::NodePtr bptree::CachedBPTree::initNode(bptree::Node::type_t t) {
    if (freelist_head == NO_FREE) {
        FUNC_CALL(file_size);
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
        if(file.fail())throw std::runtime_error("initNode()");
        freelist_head = file_size;
        file_size += Node::BLOCK_SIZE;
    }
    NodePtr ptr = cache.get(freelist_head);
    freelist_head = ptr->next;
    ptr->type = t;
    return ptr;
}

void bptree::CachedBPTree::saveNode(bptree::NodePtr node) {
    flush(file, node);
}

bptree::NodePtr bptree::CachedBPTree::loadNode(bptree::LocaType offset) {
    return cache.get(offset);
}


void bptree::CachedBPTree::deleteNode(bptree::NodePtr node) {
    node->type = Node::FREE;
    node->next = freelist_head;
    auto tmp = node->next;
    cache.remove(node->offset);
    freelist_head = tmp;
}

bool bptree::createTree(const std::string& path) {
    std::fstream f(path, ios::in | ios::out | ios::binary);
    if (f.is_open() || f.bad()) { return false; }
    f.close();
    f = std::fstream(path, ios::out | ios::binary);
#define write_attribute(ATTR) memcpy(ptr,(void*)&ATTR,sizeof(ATTR));ptr+=sizeof(ATTR)
    char buf[sizeof(CachedBPTree::file_size)+sizeof(CachedBPTree::freelist_head)+sizeof(CachedBPTree::root)];
    char* ptr = buf;
    size_t size = sizeof(buf);
    LocaType free = CachedBPTree::NO_FREE;
    LocaType t = Node::NONE;
    write_attribute(size);
    write_attribute(free);
    write_attribute(t); // root
    write_attribute(t); // sequential_head
    f.write(buf, sizeof(buf));
    f.close();
    return true;
#undef write_attribute
}

CachedBPTree::CachedBPTree(const std::string& path, size_t block_size) :
        BPTree(), file(path, ios::in | ios::out | ios::binary),
        cache(block_size, [this](LocaType o, Node* r) { load(file, o, r); }, [this](Node* o) { flush(file, o); }) {
#define read_attribute(ATTR) memcpy((void*)&ATTR,ptr,sizeof(ATTR));ptr+=sizeof(ATTR)
    if (file.bad()) {
        throw std::runtime_error("Open B+ Tree failed.");
    }
    char buf[sizeof(file_size)+sizeof(freelist_head)+sizeof(root)];
    char* ptr = buf;
    file.seekg(0);
    file.read(buf, sizeof(buf));
    read_attribute(file_size);
    read_attribute(freelist_head);
    read_attribute(root);
#undef read_attribute
}

CachedBPTree::~CachedBPTree() {
#define write_attribute(ATTR) memcpy(ptr,(void*)&ATTR,sizeof(ATTR));ptr+=sizeof(ATTR)
    char buf[sizeof(file_size)+sizeof(freelist_head)+sizeof(root)];
    char* ptr = buf;
    write_attribute(file_size);
    write_attribute(freelist_head);
    write_attribute(root);
    file.seekg(0);
    file.write(buf, sizeof(buf));
    cache.destruct();
    file.close();
#undef write_attribute
}
