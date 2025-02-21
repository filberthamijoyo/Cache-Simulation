/*
 * Basic cache simulator
 *
 * Created by He, Hao on 2019-4-27
 */

#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <vector>
#include "MemoryManager.h"

// Forward declaration of MemoryManager
class MemoryManager;

// Cache class simulating a multi-level cache system
class Cache {
public:
    // Policy structure defining cache configuration
    struct Policy {
        uint32_t cacheSize;       // Total cache size in bytes (power of 2)
        uint32_t blockSize;       // Size of each block in bytes (power of 2)
        uint32_t blockNum;        // Number of blocks in the cache
        uint32_t associativity;   // Number of blocks per set
        uint32_t hitLatency;      // Cycles for a cache hit
        uint32_t missLatency;     // Cycles for a cache miss
    };

    // Block structure representing a single cache block
    struct Block {
        bool valid;                     // Valid bit
        bool modified;                  // Modified bit for write-back
        uint32_t tag;                   // Tag portion of the address
        uint32_t id;                    // Set ID
        uint32_t size;                  // Size of the block
        uint32_t lastReference;         // LRU counter
        std::vector<uint8_t> data;      // Data stored in the block

        Block() {}
        Block(const Block &b)
            : valid(b.valid), modified(b.modified), tag(b.tag), id(b.id),
              size(b.size), lastReference(b.lastReference), data(b.data) {}
    };

    // Statistics structure tracking cache performance
    struct Statistics {
        uint32_t numRead;       // Number of read operations
        uint32_t numWrite;      // Number of write operations
        uint32_t numHit;        // Number of cache hits
        uint32_t numMiss;       // Number of cache misses
        uint64_t totalCycles;   // Total cycles consumed
    };

    // Constructor to initialize the cache
    Cache(MemoryManager *manager, Policy policy, Cache *lowerCache = nullptr,
          bool writeBack = true, bool writeAllocate = true);

    // Checks if an address is present in the cache
    bool inCache(uint32_t addr);

    // Retrieves the block ID for a given address
    uint32_t getBlockId(uint32_t addr);

    // Retrieves a byte from the cache with optional cycle count and prefetch flag
    uint8_t getByte(uint32_t addr, uint32_t *cycles = nullptr, bool is_prefetch = false);

    // Sets a byte in the cache with optional cycle count
    void setByte(uint32_t addr, uint8_t val, uint32_t *cycles = nullptr);

    // Prints cache configuration and optionally detailed block information
    void printInfo(bool verbose);

    // Prints cache access statistics
    void printStatistics();

    // Public statistics member
    Statistics statistics;

private:
    uint32_t referenceCounter;     // Global reference counter for LRU
    bool writeBack;                // Write-back policy flag
    bool writeAllocate;            // Write-allocate policy flag
    MemoryManager *memory;         // Pointer to the memory manager
    Cache *lowerCache;             // Pointer to the lower cache level
    Policy policy;                 // Cache configuration policy
    std::vector<Block> blocks;     // Vector of cache blocks

    // Initializes all cache blocks based on the policy
    void initCache();

    // Loads a block from the lower cache level or memory
    void loadBlockFromLowerLevel(uint32_t addr, uint32_t *cycles = nullptr, bool is_prefetch = false);

    // Determines which block to replace using LRU policy
    uint32_t getReplacementBlockId(uint32_t begin, uint32_t end);

    // Writes a block to the lower cache level or memory
    void writeBlockToLowerLevel(Block &b);

    // Validates the cache configuration policy
    bool isPolicyValid();

    // Checks if a number is a power of two
    bool isPowerOfTwo(uint32_t n);

    // Calculates the integer log base 2 of a value
    uint32_t log2i(uint32_t val);

    // Extracts the tag from an address
    uint32_t getTag(uint32_t addr);

    // Extracts the set ID from an address
    uint32_t getId(uint32_t addr);

    // Extracts the byte offset within a block from an address
    uint32_t getOffset(uint32_t addr);

    // Reconstructs the address from a block's tag and ID
    uint32_t getAddr(Block &b);
};

#endif#endif