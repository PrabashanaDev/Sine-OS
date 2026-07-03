#include "pmm.h"

// We support up to 128MB of RAM for this simple hobby OS.
// 128MB / 4096 bytes per page = 32768 pages.
// 32768 pages / 8 bits = 4096 bytes for the bitmap.
uint8_t memory_bitmap[4096];
size_t max_blocks = 0;
size_t used_blocks = 0;

// Set a bit in the bitmap (Mark block as USED)
static void bitmap_set(size_t bit) {
    memory_bitmap[bit / 8] |= (1 << (bit % 8));
}

// Clear a bit in the bitmap (Mark block as FREE)
static void bitmap_unset(size_t bit) {
    memory_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Test a bit in the bitmap
static int bitmap_test(size_t bit) {
    return memory_bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(size_t mem_size, uint32_t kernel_end_addr) {
    // We ignore kernel_end_addr for now and just hard-reserve the first 4MB
    (void)kernel_end_addr; 
    
    max_blocks = mem_size / PAGE_SIZE;
    used_blocks = max_blocks;
    
    // By default, mark all memory as IN USE (1) to prevent accidental overwrites
    memset(memory_bitmap, 0xFF, sizeof(memory_bitmap));
    
    // Now, mark the actual available RAM as FREE (0)
    for (size_t i = 0; i < max_blocks; i++) {
        bitmap_unset(i);
        used_blocks--;
    }
    
    // Mark the memory used by the kernel itself (and BIOS/GRUB at the bottom) as IN USE.
    // We will reserve the first 4 Megabytes just to be extremely safe for our kernel.
    size_t reserved_blocks = (4 * 1024 * 1024) / PAGE_SIZE; 
    for (size_t i = 0; i < reserved_blocks; i++) {
        bitmap_set(i);
        used_blocks++;
    }
}

// Finds the first free block and returns its index
static int pmm_find_first_free() {
    for (size_t i = 0; i < max_blocks / 32; i++) {
        uint32_t* chunk = (uint32_t*) &memory_bitmap[i * 4];
        if (*chunk != 0xFFFFFFFF) { // If chunk is not entirely full
            for (int j = 0; j < 32; j++) {
                int bit = i * 32 + j;
                if (!bitmap_test(bit)) {
                    return bit;
                }
            }
        }
    }
    return -1; // Out of memory
}

uint32_t pmm_alloc_block() {
    if (used_blocks >= max_blocks) return 0; // Out of memory
    
    int free_block = pmm_find_first_free();
    if (free_block == -1) return 0;
    
    bitmap_set(free_block);
    used_blocks++;
    
    return free_block * PAGE_SIZE; // Return the physical memory address!
}

void pmm_free_block(uint32_t addr) {
    size_t block = addr / PAGE_SIZE;
    if (bitmap_test(block)) {
        bitmap_unset(block);
        used_blocks--;
    }
}
