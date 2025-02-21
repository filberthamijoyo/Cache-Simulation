/**
 * Entry point for the optimized cache
 *
 * Created by He, Hao at 2019/04/30
 */

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Cache.h"
#include "Debug.h"
#include "MemoryManager.h"

// Function to parse command-line parameters
bool parseParameters(int argc, char **argv);

// Function to display usage instructions
void printUsage();

// Global variable for trace file path
const char *traceFilePath;

int main(int argc, char **argv) {
    // Parse input parameters
    if (!parseParameters(argc, argv)) {
        return -1;
    }

    // Define cache policies for L1, L2, and L3 caches
    Cache::Policy l1policy = {16 * 1024, 64, (16 * 1024) / 64, 1, 1, 0};
    Cache::Policy l2policy = {128 * 1024, 64, (128 * 1024) / 64, 8, 8, 0};
    Cache::Policy l3policy = {2 * 1024 * 1024, 64, (2 * 1024 * 1024) / 64, 16, 20, 100};

    // Initialize memory manager and cache hierarchy
    MemoryManager *memory = new MemoryManager();
    Cache *l3cache = new Cache(memory, l3policy, nullptr, true, true);
    Cache *l2cache = new Cache(memory, l2policy, l3cache, true, true);
    Cache *l1cache = new Cache(memory, l1policy, l2cache, true, true);
    memory->setCache(l1cache);

    // Open the trace file for reading
    std::ifstream trace(traceFilePath);
    if (!trace.is_open()) {
        printf("Unable to open file %s\n", traceFilePath);
        exit(-1);
    }

    char type;            // Operation type: 'r' for read, 'w' for write
    uint32_t addr;        // Address for the operation
    uint32_t last_addr = 0;    // Previous address for stride calculation
    int64_t stride = 0;         // Current stride value
    bool is_prefetch = false;   // Prefetching flag
    int same_stride_count = 0;  // Count of consistent strides
    int diff_stride_count = 0;  // Count of inconsistent strides during prefetch

    // Process each operation in the trace file
    while (trace >> type >> std::hex >> addr) {
        // Ensure the memory page exists
        if (!memory->isPageExist(addr)) {
            memory->addPage(addr);
        }

        // Perform the read or write operation
        switch (type) {
            case 'r':
                memory->getByte(addr);
                break;
            case 'w':
                memory->setByte(addr, 0);
                break;
            default:
                dbgprintf("Illegal type %c\n", type);
                exit(-1);
        }

        // Calculate the stride between current and last address
        int64_t new_stride = static_cast<int64_t>(addr) - static_cast<int64_t>(last_addr);
        last_addr = addr;

        if (!is_prefetch) {
            // Check for consistent stride
            if (new_stride == stride) {
                same_stride_count++;
            } else {
                stride = new_stride;
                same_stride_count = 1;
            }

            // Enable prefetching after consistent strides
            if (same_stride_count >= 3) {
                is_prefetch = true;
                diff_stride_count = 0;

                // Prefetch the next 3 blocks
                for (int i = 1; i <= 3; ++i) {
                    uint32_t prefetch_addr = addr + i * stride;

                    if (!l1cache->inCache(prefetch_addr)) {
                        if (!memory->isPageExist(prefetch_addr)) {
                            memory->addPage(prefetch_addr);
                        }
                        l1cache->getByte(prefetch_addr, nullptr, true);
                    }
                }
            }
        } else {
            // Prefetching is active: verify stride consistency
            if (new_stride == stride) {
                diff_stride_count = 0;

                // Continue prefetching the next 2 blocks
                for (int i = 1; i <= 2; ++i) {
                    uint32_t prefetch_addr = addr + i * stride;

                    if (!l1cache->inCache(prefetch_addr)) {
                        if (!memory->isPageExist(prefetch_addr)) {
                            memory->addPage(prefetch_addr);
                        }
                        l1cache->getByte(prefetch_addr, nullptr, true);
                    }
                }
            } else {
                // Increment inconsistency count and potentially disable prefetching
                diff_stride_count++;
                if (diff_stride_count > 3) {
                    is_prefetch = false;
                    stride = new_stride;
                    same_stride_count = 1;
                }
            }
        }
    }

    // Display cache statistics for L1 cache
    printf("L1 Cache:\n");
    l1cache->printStatistics();

    // Clean up allocated memory
    delete l1cache;
    delete l2cache;
    delete l3cache;
    delete memory;

    return 0;
}

// Parses command-line arguments to retrieve the trace file path
bool parseParameters(int argc, char **argv) {
    if (argc > 1) {
        traceFilePath = argv[1];
        return true;
    } else {
        return false;
    }
}

// Displays usage instructions for the program
void printUsage() {
    printf("Usage: CacheSim trace-file\n");
}