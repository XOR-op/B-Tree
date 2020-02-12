cmake_minimum_required(VERSION 3.16)
project(BPtree)

set(CMAKE_CXX_STANDARD 14)

add_executable(BPtree bptree.h bptree.cpp debug.h CachedBPtree.h Cache.h public.h CachedBPtree.cpp main.cpp)