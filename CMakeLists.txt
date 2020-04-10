cmake_minimum_required(VERSION 3.16)
project(BPtree)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2")

aux_source_directory(src SOURCE)
add_executable(BPtree ${SOURCE} src/cache.h src/analysis.h)