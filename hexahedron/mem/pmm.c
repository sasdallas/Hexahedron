/**
 * @file hexahedron/mem/pmm.c
 * @brief Physical memory manager 
 * 
 * This is the default Hexahedron PMM. It is a bitmap frame allocator.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>

// Kernel includes
#include <kernel/mem/pmm.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

// Frames bitmap 
uintptr_t    *frames;
uintptr_t    nframes = 0;

// Debugging information
uintptr_t    pmm_memorySize = 0;
uintptr_t    pmm_usedBlocks = 0;
uintptr_t    pmm_maxBlocks = 0;

/**
 * @brief Initialize the physical memory system.
 * @param memsize Available physical memory in bytes.
 * @param frames_bitmap The bitmap of frames allocated to the system (mapped into memory)
 */
int pmm_init(uintptr_t memsize, uintptr_t *frames_bitmap) {
    pmm_memorySize = memsize;
    pmm_maxBlocks = pmm_memorySize / PMM_BLOCK_SIZE;
    pmm_usedBlocks = pmm_maxBlocks; // By default, all is in use. You must mark valid memory manually

    frames = frames_bitmap;
    nframes = pmm_maxBlocks;

    // Set the bitmap to be all in use by default
    memset(frames, 0xF, nframes / 8);
}

// Set a frame/bit in the frames array
void pmm_setFrame(int frame) {
    frames[PMM_INDEX_BIT(frame)] |= (1 << PMM_OFFSET_BIT(frame));
}

// Clear a frame/bit in the frames array
void pmm_clearFrame(int frame) {
    frames[PMM_INDEX_BIT(frame)] &= ~(1 << PMM_OFFSET_BIT(frame));
}

// Test whether a frame is set
int pmm_testFrame(int frame) {
    return frames[PMM_INDEX_BIT(frame)] & (1 << PMM_OFFSET_BIT(frame));
}

// Find the first free frame
uint32_t pmm_findFirstFrame() {
    for (uint32_t i = 0; i < PMM_INDEX_BIT(nframes); i++) {
        if (frames[i] != 0xFFFFFFFF) {
            // At least one bit is free. Which one?
            for (uint32_t bit = 0; bit < 32; bit++) {
                if (!(frames[i] & (1 << bit))) {
                    return i * 4 * 8 + bit;
                }
            }
        }
    }

    // No free frame
    return -ENOMEM;
}

// Find first n frames
int pmm_findFirstFrames(size_t n) {
    if (n == 0) return 0x0;
    if (n == 1) return pmm_findFirstFrame();

    for (uint32_t i = 0; i < PMM_INDEX_BIT(nframes); i++) {
        if (frames[i] != 0xFFFFFFFF) {
            // At least one bit is free. Which one?
            for (uint32_t bit = 0; bit < 32; bit++) {
                if (!(frames[i] & (1 << bit))) {
                    // Found it! But are there enough?

                    int starting_bit = i * 32 + bit;

                    uint32_t free_bits = 0;
                    for (uint32_t check = 0; check < n; check++) {
                        if (!pmm_testFrame(starting_bit + check)) free_bits++;
                        if (free_bits == n) return i * 4 * 8 + bit; // Yes, there are!
                    }
                }
            }
        }
    }

    // Not enough free frames
    return -ENOMEM;
}

/**
 * @brief Initialize a region as available memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_initializeRegion(uintptr_t base, uintptr_t size) {
    if (!size) return;

    // Careful not to cause a div by zero on base.
    int align = (base == 0x0) ? 0x0: base / PMM_BLOCK_SIZE;
    int blocks = size / PMM_BLOCK_SIZE;

    for (; blocks > 0; blocks--) {
        pmm_clearFrame(align++);
        pmm_usedBlocks--;
    }
}

/**
 * @brief Initialize a region as unavailable memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_deinitializeRegion(uintptr_t base, uintptr_t size) {
    if (!size) return;

    // Careful not to cause a div by zero on base.
    int align = (base == 0x0) ? 0x0: base / PMM_BLOCK_SIZE;
    int blocks = size / PMM_BLOCK_SIZE;

    for (; blocks > 0; blocks--) {
        pmm_setFrame(align++);
        pmm_usedBlocks++;
    }
}

/**
 * @brief Allocate a block
 * @returns A pointer to the block. If we run out of memory it will critically fault
 */
uintptr_t pmm_allocateBlock() {
    if ((pmm_maxBlocks - pmm_usedBlocks) <= 0) {
        goto _oom;
    }

    int frame = pmm_findFirstFrame();
    if (frame == -ENOMEM) goto _oom;

    pmm_setFrame(frame);
    pmm_usedBlocks++;
    
    return (uintptr_t)(frame * PMM_BLOCK_SIZE);

_oom:
    kernel_panic(OUT_OF_MEMORY, "physmem");
    __builtin_unreachable();
}

/**
 * @brief Frees a block
 * @param block The address of the block
 */
void pmm_freeBlock(uintptr_t block) {
    if (!block || block % PMM_BLOCK_SIZE != 0) return;

    int frame = block / PMM_BLOCK_SIZE;
    pmm_clearFrame(frame);
    pmm_usedBlocks--;
}

/**
 * @brief Allocate @c blocks amount of blocks
 * @param blocks The amount of blocks to allocate. Must be aligned to PMM_BLOCK_SIZE - do not pass bytes!
 */
uintptr_t pmm_allocateBlocks(size_t blocks) {
    if (!blocks) kernel_panic(KERNEL_BAD_ARGUMENT_ERROR, "physmem");

    if ((pmm_maxBlocks - pmm_usedBlocks) <= blocks) kernel_panic(OUT_OF_MEMORY, "physmem");

    int frame = pmm_findFirstFrames(blocks);
    if (frame == -ENOMEM) kernel_panic(OUT_OF_MEMORY, "physmem");

    for (uint32_t i = 0; i < blocks; i++) {
        pmm_setFrame(frame + i);
    }

    pmm_usedBlocks += blocks;
    return (uintptr_t)(frame * 4096);
}

/**
 * @brief Frees @c blocks amount of blocks
 * @param base Pointer returned by @c pmm_allocateBlocks
 * @param blocks The amount of blocks to free. Must be aligned to PMM_BLOCK_SIZE
 */
void pmm_freeBlocks(uintptr_t base, size_t blocks) {
    if (!blocks) return;
    
    // Careful not to cause a div by zero on base.
    int align = (base == 0x0) ? 0x0: base / PMM_BLOCK_SIZE;
    
    for (uint32_t i = 0; i < blocks; i++) pmm_clearFrame(base + i);

    pmm_usedBlocks -= blocks;
}

/**
 * @brief Gets the physical memory size
 */
uintptr_t pmm_getPhysicalMemorySize() {
    return pmm_memorySize;
}

/**
 * @brief Gets the maximum amount of blocks
 */
uintptr_t pmm_getMaximumBlocks() {
    return pmm_maxBlocks;
}

/**
 * @brief Gets the used amount of blocks
 */
uintptr_t pmm_getUsedBlocks() {
    return pmm_usedBlocks;
}

/**
 * @brief Gets the free amount of blocks
 */
uintptr_t pmm_getFreeBlocks() {
    return (pmm_maxBlocks - pmm_usedBlocks);
}