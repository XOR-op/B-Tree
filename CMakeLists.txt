cmake_minimum_required(VERSION 3.16)
project(BPtree)

set(CMAKE_CXX_STANDARD 17)

aux_source_directory(src SOURCE)
add_executable(BPtree ${SOURCE} include/algorithm.h)