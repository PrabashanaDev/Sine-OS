#ifndef PMM_H
#define PMM_H

#include "string.h" // For size_t and standard types

#define PAGE_SIZE 4096
#define BLOCKS_PER_BYTE 8

// Initialize the Physical Memory Manager
void pmm_init(size_t mem_size, uint32_t bitmap_addr);

// Allocate a single 4KB block of physical memory
uint32_t pmm_alloc_block();

// Free a single 4KB block of physical memory
void pmm_free_block(uint32_t addr);

#endif
