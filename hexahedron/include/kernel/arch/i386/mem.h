/**
 * @file hexahedron/include/kernel/arch/i386/mem.h
 * @brief i386-specific memory systems
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_I386_MEM_H
#define KERNEL_ARCH_I386_MEM_H

/**** INCLUDES ****/
#include <stdint.h>


/**** TYPES ****/

typedef uint32_t pde_t; // Page directory entry
typedef uint32_t pte_t; // Page table entry

// Page directory and page table structures.
// THESE ARE LEGACY. USE "union page"
typedef struct _page_directory {
    pde_t entries[1024];
} page_directory_t;

typedef struct _page_table {
    pte_t entries[1024];
} page_table_t;


typedef union page {
    // You can manually manipulate these flags 
    struct {
        // 4MB paging is not used.
        uint32_t present:1;
        uint32_t rw:1;
        uint32_t usermode:1;
        uint32_t writethrough:1;
        uint32_t cache_disable:1;
        uint32_t accessed:1;
        uint32_t dirty:1;
        uint32_t pat:1;
        uint32_t global:1;
        uint32_t available:3;
        uint32_t address:20;
    } bits;

    // Or you can manipulate the page raw
    uint32_t data;
} page_t;

/**** DEFINITIONS ****/

// PDE bitflags
#define PDE_PRESENT         0x01
#define PDE_WRITABLE        0x02
#define PDE_USER            0x04
#define PDE_PWT             0x08
#define PDE_PCD             0x10
#define PDE_ACCESSED        0x20
#define PDE_DIRTY           0x40
#define PDE_4MB             0x80
#define PDE_CPU_GLOBAL      0x100
#define PDE_LV4_GLOBAL      0x200
#define PDE_FRAME           0x7FFFF000

// PTE bitflags
#define PTE_PRESENT         0x01
#define PTE_WRITABLE        0x02
#define PTE_USER            0x04
#define PTE_PWT             0x08
#define PTE_PCD             0x10
#define PTE_ACCESSED        0x20
#define PTE_DIRTY           0x40
#define PTE_4MB             0x80
#define PTE_CPU_GLOBAL      0x100
#define PTE_LV4_GLOBAL      0x200
#define PTE_FRAME           0x7FFFF000


// General page defitions
#define PAGE_SIZE   4096

// CR0/CR4 paging bits
#define CR4_PSE_BIT     0x00000010  // PSE (4MB pages) bit
#define CR0_PG_PE_BIT   0x80000001  // PG/PE bit for enabling paging
#define CR0_WP_BIT      0x00010000  // Write-protect bit. Allows for CoW
#define CR0_PG_BIT      0x80000000  // PG bit for disabling paging

// Page shifting
#define MEM_PAGE_SHIFT  12


// IMPORTANT: THIS IS THE HEXAHEDRON MEMORY MAP CONFIGURED FOR I386
// 0x00000000 - 0x00200000: Kernel code. This can be expanded since heap is positioned right after
// 0x00200000 - 0x00400000: Kernel heap. This is just an example heap.
// 0xA0000000 - 0xB0000000: Driver memory space.
// 0xB0000000 - 0xF0000000: Physical memory. This is a space of memory used for identity mapping
// 0xFD000000 - 0xFFFFFFFF: Framebuffer for the kernel. It can be unmapped for later usage if needed.

#define MEM_IDENTITY_MAP_REGION     0xB0000000
#define MEM_IDENTITY_MAP_SIZE       0x30000000
#define MEM_FRAMEBUFFER_REGION      0xFD000000


/**** MACROS ****/

#define MEM_ALIGN_PAGE(addr) ((addr & 0xFFFFF000) + 4096) // Align an address to the nearest page
#define MEM_PAGEDIR_INDEX(x) (((x) >> 22) & 0x3ff) // Returns the index of x within the page directory
#define MEM_PAGETBL_INDEX(x) (((x) >> 12) & 0x3ff) // Returns the index of x within the page table
#define MEM_VIRTUAL_TO_PHYS(addr) (*addr & ~0xFFF) // Returns the physical frame address of a page.

#define MEM_SET_FRAME(page, frame) (page->bits.address = ((uintptr_t)frame >> MEM_PAGE_SHIFT))      // Set the frame of a page. Used because of our weird union thing.
#define MEM_GET_FRAME(page) (page->bits.address << MEM_PAGE_SHIFT)                                  // Get the frame of a page. Used because of our weird union thing.





#endif