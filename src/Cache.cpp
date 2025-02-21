/*
 * Implementation of a simple cache simulator
 *
 * Created By He, Hao in 2019-04-27
 * Modified on 2024-12-11 to support prefetching
 */

#include <cstdio>
#include <cstdlib>
#include "Cache.h"

// Constructor: Initializes the cache with given parameters
Cache::Cache(MemoryManager *manager, Policy policy, Cache *lowerCache,
             bool writeBack, bool writeAllocate) {
    referenceCounter = 0;
    memory = manager;
    this->policy = policy;
    this->lowerCache = lowerCache;

    if (!isPolicyValid()) {
        fprintf(stderr, "Policy invalid!\n");
        exit(-1);
    }

    initCache();
    statistics = Statistics{0, 0, 0, 0, 0};
    this->writeBack = writeBack;
    this->writeAllocate = writeAllocate;
}

// Checks if the address is present in the cache
bool Cache::inCache(uint32_t addr) {
    return getBlockId(addr) != -1;
}

// Retrieves the block ID for a given address
uint32_t Cache::getBlockId(uint32_t addr) {
    uint32_t tag = getTag(addr);
    uint32_t id = getId(addr);

    for (uint32_t i = id * policy.associativity; i < (id + 1) * policy.associativity; ++i) {
        if (blocks[i].id != id) {
            fprintf(stderr, "Inconsistent ID in block %d\n", i);
            exit(-1);
        }
        if (blocks[i].valid && blocks[i].tag == tag) {
            return i;
        }
    }
    return -1;
}

// Retrieves a byte from the cache, updating cycles and handling misses
uint8_t Cache::getByte(uint32_t addr, uint32_t *cycles, bool is_prefetch) {
    referenceCounter++;
    if (!is_prefetch) {
        statistics.numRead++;
    }

    int blockId = getBlockId(addr);
    if (blockId != -1) {
        uint32_t offset = getOffset(addr);
        statistics.numHit++;
        statistics.totalCycles += policy.hitLatency;
        blocks[blockId].lastReference = referenceCounter;
        if (cycles) *cycles = policy.hitLatency;
        return blocks[blockId].data[offset];
    }

    if (!is_prefetch) {
        statistics.numMiss++;
        statistics.totalCycles += policy.missLatency;
    }

    loadBlockFromLowerLevel(addr, cycles, is_prefetch);

    blockId = getBlockId(addr);
    if (blockId != -1) {
        uint32_t offset = getOffset(addr);
        blocks[blockId].lastReference = referenceCounter;
        return blocks[blockId].data[offset];
    } else {
        fprintf(stderr, "Error: data not in top level cache!\n");
        exit(-1);
    }
}

// Sets a byte in the cache, handling write policies and misses
void Cache::setByte(uint32_t addr, uint8_t val, uint32_t *cycles) {
    referenceCounter++;
    statistics.numWrite++;

    int blockId = getBlockId(addr);
    if (blockId != -1) {
        uint32_t offset = getOffset(addr);
        statistics.numHit++;
        statistics.totalCycles += policy.hitLatency;
        blocks[blockId].modified = true;
        blocks[blockId].lastReference = referenceCounter;
        blocks[blockId].data[offset] = val;
        if (!writeBack) {
            writeBlockToLowerLevel(blocks[blockId]);
            statistics.totalCycles += policy.missLatency;
        }
        if (cycles) *cycles = policy.hitLatency;
        return;
    }

    statistics.numMiss++;
    statistics.totalCycles += policy.missLatency;

    if (writeAllocate) {
        loadBlockFromLowerLevel(addr, cycles);
        blockId = getBlockId(addr);
        if (blockId != -1) {
            uint32_t offset = getOffset(addr);
            blocks[blockId].modified = true;
            blocks[blockId].lastReference = referenceCounter;
            blocks[blockId].data[offset] = val;
            return;
        } else {
            fprintf(stderr, "Error: data not in top level cache!\n");
            exit(-1);
        }
    } else {
        if (lowerCache == nullptr) {
            memory->setByteNoCache(addr, val);
        } else {
            lowerCache->setByte(addr, val);
        }
    }
}

// Prints cache configuration and optionally block details
void Cache::printInfo(bool verbose) {
    printf("---------- Cache Info -----------\n");
    printf("Cache Size: %d bytes\n", policy.cacheSize);
    printf("Block Size: %d bytes\n", policy.blockSize);
    printf("Block Num: %d\n", policy.blockNum);
    printf("Associativity: %d\n", policy.associativity);
    printf("Hit Latency: %d cycles\n", policy.hitLatency);
    printf("Miss Latency: %d cycles\n", policy.missLatency);

    if (verbose) {
        for (int j = 0; j < blocks.size(); ++j) {
            const Block &b = blocks[j];
            printf("Block %d: tag 0x%x id %d %s %s (last ref %d)\n",
                   j, b.tag, b.id,
                   b.valid ? "valid" : "invalid",
                   b.modified ? "modified" : "unmodified",
                   b.lastReference);
        }
    }
}

// Displays cache access statistics
void Cache::printStatistics() {
    printf("-------- STATISTICS ----------\n");
    printf("Num Read: %d\n", statistics.numRead);
    printf("Num Write: %d\n", statistics.numWrite);
    printf("Num Hit: %d\n", statistics.numHit);
    printf("Num Miss: %d\n", statistics.numMiss);
    printf("Total Cycles: %llu\n", statistics.totalCycles);
    if (lowerCache != nullptr) {
        printf("---------- LOWER CACHE ----------\n");
        lowerCache->printStatistics();
    }
}

// Validates the cache configuration policy
bool Cache::isPolicyValid() {
    if (!isPowerOfTwo(policy.cacheSize)) {
        fprintf(stderr, "Invalid Cache Size %d\n", policy.cacheSize);
        return false;
    }
    if (!isPowerOfTwo(policy.blockSize)) {
        fprintf(stderr, "Invalid Block Size %d\n", policy.blockSize);
        return false;
    }
    if (policy.cacheSize % policy.blockSize != 0) {
        fprintf(stderr, "cacheSize %% blockSize != 0\n");
        return false;
    }
    if (policy.blockNum * policy.blockSize != policy.cacheSize) {
        fprintf(stderr, "blockNum * blockSize != cacheSize\n");
        return false;
    }
    if (policy.blockNum % policy.associativity != 0) {
        fprintf(stderr, "blockNum %% associativity != 0\n");
        return false;
    }
    return true;
}

// Initializes all cache blocks based on the policy
void Cache::initCache() {
    blocks = std::vector<Block>(policy.blockNum);
    for (uint32_t i = 0; i < blocks.size(); ++i) {
        Block &b = blocks[i];
        b.valid = false;
        b.modified = false;
        b.size = policy.blockSize;
        b.tag = 0;
        b.id = i / policy.associativity;
        b.lastReference = 0;
        b.data = std::vector<uint8_t>(b.size);
    }
}

// Loads a block from the lower cache level or memory
void Cache::loadBlockFromLowerLevel(uint32_t addr, uint32_t *cycles, bool is_prefetch) {
    uint32_t blockSize = policy.blockSize;
    Block newBlock;
    newBlock.valid = true;
    newBlock.modified = false;
    newBlock.tag = getTag(addr);
    newBlock.id = getId(addr);
    newBlock.size = blockSize;
    newBlock.data = std::vector<uint8_t>(blockSize);

    uint32_t blockAddrBegin = addr & ~(blockSize - 1);

    if (lowerCache != nullptr) {
        for (uint32_t i = blockAddrBegin; i < blockAddrBegin + 1; ++i) {
            newBlock.data[i - blockAddrBegin] = lowerCache->getByte(i, cycles, is_prefetch);
        }
    } else {
        for (uint32_t i = blockAddrBegin; i < blockAddrBegin + 1; ++i) {
            newBlock.data[i - blockAddrBegin] = memory->getByteNoCache(i);
            if (cycles) *cycles += 100;
        }
    }

    uint32_t id = getId(addr);
    uint32_t blockIdBegin = id * policy.associativity;
    uint32_t blockIdEnd = (id + 1) * policy.associativity;
    uint32_t replaceId = getReplacementBlockId(blockIdBegin, blockIdEnd);

    Block replaceBlock = blocks[replaceId];
    if (writeBack && replaceBlock.valid && replaceBlock.modified) {
        writeBlockToLowerLevel(replaceBlock);
        statistics.totalCycles += policy.missLatency;
    }

    blocks[replaceId] = newBlock;
}

// Determines which block to replace using LRU policy
uint32_t Cache::getReplacementBlockId(uint32_t begin, uint32_t end) {
    for (uint32_t i = begin; i < end; ++i) {
        if (!blocks[i].valid)
            return i;
    }

    uint32_t resultId = begin;
    uint32_t minReference = UINT32_MAX;
    for (uint32_t i = begin; i < end; ++i) {
        if (blocks[i].lastReference < minReference) {
            resultId = i;
            minReference = blocks[i].lastReference;
        }
    }
    return resultId;
}

// Writes a block to the lower cache level or memory
void Cache::writeBlockToLowerLevel(Cache::Block &b) {
    uint32_t addrBegin = getAddr(b);
    if (lowerCache == nullptr) {
        for (uint32_t i = 0; i < b.size; ++i) {
            memory->setByteNoCache(addrBegin + i, b.data[i]);
        }
    } else {
        for (uint32_t i = 0; i < b.size; ++i) {
            lowerCache->setByte(addrBegin + i, b.data[i]);
        }
    }
}

// Checks if a number is a power of two
bool Cache::isPowerOfTwo(uint32_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// Calculates the integer log base 2 of a value
uint32_t Cache::log2i(uint32_t val) {
    if (val == 0)
        return uint32_t(-1);
    if (val == 1)
        return 0;
    uint32_t ret = 0;
    while (val > 1) {
        val >>= 1;
        ret++;
    }
    return ret;
}

// Extracts the tag from an address
uint32_t Cache::getTag(uint32_t addr) {
    uint32_t offsetBits = log2i(policy.blockSize);
    uint32_t idBits = log2i(policy.blockNum / policy.associativity);
    uint32_t mask = (1 << (32 - offsetBits - idBits)) - 1;
    return (addr >> (offsetBits + idBits)) & mask;
}

// Extracts the set ID from an address
uint32_t Cache::getId(uint32_t addr) {
    uint32_t offsetBits = log2i(policy.blockSize);
    uint32_t idBits = log2i(policy.blockNum / policy.associativity);
    uint32_t mask = (1 << idBits) - 1;
    return (addr >> offsetBits) & mask;
}

// Extracts the byte offset within a block from an address
uint32_t Cache::getOffset(uint32_t addr) {
    uint32_t bits = log2i(policy.blockSize);
    uint32_t mask = (1 << bits) - 1;
    return addr & mask;
}

// Reconstructs the address from a block's tag and ID
uint32_t Cache::getAddr(Cache::Block &b) {
    uint32_t offsetBits = log2i(policy.blockSize);
    uint32_t idBits = log2i(policy.blockNum / policy.associativity);
    return (b.tag << (offsetBits + idBits)) | (b.id << offsetBits);
}