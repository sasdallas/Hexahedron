/**
 * @file hexahedron/include/kernel/mem/mem.h
 * @brief Memory system functions
 * 
 * This file is an interface for the memory mapper
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_H
#define KERNEL_MEM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>

#if defined(__ARCH_I386__) || defined(__INTELLISENSE__)
#include <kernel/arch/i386/mem.h> // Arch-specific definitions, like directory, entries, etc)
#else
#error "Unsupported architecture for memory management"
#endif



/**** DEFINITIONS ****/

// Flags for the memory mapper
// TODO: These are kinda gross.
#define MEM_DEFAULT             0x00    // Default settings to memory allocator (usermode, writable, and present)
#define MEM_CREATE              0x01    // Create the page. Commonly used with mem_getPage during mappings
#define MEM_KERNEL              0x02    // The page is kernel-mode only
#define MEM_READONLY            0x04    // The page is read-only
#define MEM_WRITETHROUGH        0x08    // The page is marked as writethrough
#define MEM_NOT_CACHEABLE       0x10    // The page is not cacheable
#define MEM_NOT_PRESENT         0x20    // The page is not present in memory
#define MEM_NOALLOC             0x40    // Do not allocate the page and instead use what was given
#define MEM_FREE_PAGE           0x80    // Free the page. Sets it to zero if specified in mem_allocatePage

/**** FUNCTIONS ****/

/**
 * @brief Get the physical address of a virtual address
 * @param dir Can be NULL to use the current directory.
 * @param virtaddr The virtual address
 * 
 * @returns NULL on a PDE not being present or the address
 */
uintptr_t mem_getPhysicalAddress(page_t *dir, uintptr_t virtaddr);

/**
 * @brief Returns the page entry requested
 * @param dir The directory to search. Specify NULL for current directory
 * @param addr The virtual address of the page
 * @param flags The flags of the page to look for
 * 
 * @warning Specifying MEM_CREATE will only create needed structures, it will NOT allocate the page!
 *          Please use a function such as mem_allocatePage to do that.
 */
page_t *mem_getPage(page_t *dir, uintptr_t addr, uintptr_t flags);

/**
 * @brief Switch the memory management directory
 * @param pagedir The page directory to switch to
 * 
 * @warning Pass something mapped by mem_clone() or something in the identity-mapped PMM region.
 *          Anything greater than IDENTITY_MAP_MAXSIZE will be truncated in the PDBR.
 * 
 * @returns -EINVAL on invalid, 0 on success.
 */
int mem_switchDirectory(page_t *pagedir);

/**
 * @brief Get the kernel page directory
 */
page_t *mem_getKernelDirectory();


/**
 * @brief Map a physical address to a virtual address
 * 
 * @param dir The directory to map them in (leave blank for current)
 * @param phys The physical address
 * @param virt The virtual address
 */
void mem_mapAddress(page_t *dir, uintptr_t phys, uintptr_t virt);

/**
 * @brief Allocate a page using the physical memory manager
 * 
 * @param page The page object to use. Can be obtained with mem_getPage
 * @param flags The flags to follow when setting up the page
 * 
 * @note You can also use this function to set bits of a specific page - just specify @c MEM_NOALLOC in @p flags.
 * @warning The function will automatically allocate a PMM block if NOALLOC isn't specified and there isn't a frame already set.
 */
void mem_allocatePage(page_t *page, uintptr_t flags);

/**
 * @brief Remap a physical memory manager address to the identity mapped region
 * @param frame_address The address of the frame to remap
 */
uintptr_t mem_remapPhys(uintptr_t frame_address);

/**
 * @brief Die in the cold winter
 * @param bytes How many bytes were trying to be allocated
 * @param seq The sequence of failure
 */
void mem_outofmemory(int bytes, char *seq);

/**
 * @brief Get the current page directory
 */
page_t *mem_getCurrentDirectory();

/**
 * @brief Clone a page directory.
 * 
 * This is a full PROPER page directory clone. The old one just memcpyd the pagedir.
 * This function does it properly and clones the page directory, its tables, and their respective entries fully.
 * 
 * @param pd_in The source page directory. Keep as NULL to clone the current page directory.
 * @returns The page directory on success
 */
page_directory_t *mem_clone(page_directory_t *pd_in);

/**
 * @brief Initialize the memory management subsystem
 * 
 * This function will identity map the kernel into memory and setup page tables.
 */
void mem_init(uintptr_t high_address);

/**
 * @brief Free a page
 * 
 * @param page The page to free 
 */
void mem_freePage(page_t *page);


/**
 * @brief Expand/shrink the kernel heap
 * 
 * @param b The amount of bytes to allocate/free, needs to a multiple of PAGE_SIZE
 * @returns Address of the start of the bytes when allocating, or the previous address when shrinking
 */
uintptr_t mem_sbrk(int b);

/**
 * @brief Enable/disable paging
 * @param status Enable/disable
 */
void mem_setPaging(bool status);

#endif