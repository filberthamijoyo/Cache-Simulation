cmake_minimum_required(VERSION 3.5)

project(Cache-Simulator)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O2 -Wall")

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(
    CacheSingle 
    src/MainSinCache.cpp 
    src/MemoryManager.cpp 
    src/Cache.cpp
)

add_executable(
    CacheMulti
    src/MainMulCache.cpp
    src/MemoryManager.cpp
    src/Cache.cpp
)
