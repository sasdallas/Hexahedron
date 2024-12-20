/**
 * @file hexahedron/include/kernel/arch/x86_64/mem.h
 * @brief x86_64-specific memory systems
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_MEM_H
#define KERNEL_ARCH_X86_64_MEM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** TYPES ****/

typedef union page {
    // You can manually manipulate these flags
    struct {
        uint64_t present:1;         // Present in memory
        uint64_t writable:1;        // R/W
        uint64_t user:1;            // Usermode
        uint64_t writethrough:1;    // Writethrough
        uint64_t nocache:1;         // Uncacheable
        uint64_t accessed:1;        // Accessed
        uint64_t available1:1;      // Free bits!
        uint64_t size:1;            // Page size - 4MiB or 4KiB (4MiB paging is for loser. We go with no PSE!)
        uint64_t global:1;          // Global
        uint64_t available2:3;      // Free bits!
        uint64_t page:28;           // The page data
        uint64_t reserved:12;       // These should be set to 0
        uint64_t available3:11;     // Free bits!
        uint64_t nx:1;              // No execute bit
    } bits;
    
    // Or use the raw flags
    uint64_t raw;
} page_t;

/**** DEFINITIONS ****/

#define PAGE_SIZE 4096 // 4 KB ftw

// A memory map for x86_64 has not been created yet.
#define MEM_FRAMEBUFFER_REGION 0xFD000000


/**** MACROS ****/

#define MEM_ALIGN_PAGE(addr) ((addr + PAGE_SIZE) & ~0xFFF) // Align an address to the nearest page


#endif