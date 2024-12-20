/**
 * @file hexahedron/arch/x86_64/arch.c
 * @brief Architecture startup header for x86_64
 * 
 * This file handles the beginning initialization of everything specific to this architecture.
 * For x86_64, it sets up things like interrupts, TSSes, SMP cores, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


// Polyhedron
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// General
#include <kernel/config.h>
#include <kernel/multiboot.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/pmm.h>
#include <kernel/generic_mboot.h>
#include <kernel/misc/spinlock.h>

// Architecture-specific
#include <kernel/arch/x86_64/arch.h>
#include <kernel/arch/x86_64/hal.h>

/* Parameters */
generic_parameters_t *parameters = NULL;

/**
 * @brief Say hi! Prints the versioning message and ASCII art to NOHEADER dprintf
 */
void arch_say_hello(int is_debug) {

    if (!is_debug) {
        printf("Hexahedron %d.%d.%d-%s-%s (codename \"%s\")\n", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower, 
                    __kernel_architecture,
                    __kernel_build_configuration,
                    __kernel_version_codename);

        printf("%i system processors - %i KB of RAM\n", 1, parameters->mem_size);

        // NOTE: This should only be called once, so why not just modify some parameters?
        // parameters->cpu_count = smp_getCPUCount();
        printf("this is a mental note to remind me to uncomment the above line\n");

        return;
    }

    // Print out a hello message
    dprintf(NOHEADER, "%s\n", __kernel_ascii_art_formatted);
    dprintf(NOHEADER, "Hexahedron %d.%d.%d-%s-%s (codename \"%s\")\n", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower, 
                    __kernel_architecture,
                    __kernel_build_configuration,
                    __kernel_version_codename);
    
    dprintf(NOHEADER, "\tCompiled by %s on %s %s\n\n", __kernel_compiler, __kernel_build_date, __kernel_build_time);
}


/**
 * @brief Prepare the architecture to enter a fatal state. This means cleaning up registers,
 * moving things around, whatever you need to do
 */
void arch_panic_prepare() {
    dprintf(ERR, "Fatal panic state detected - please wait, cleaning up...\n");
}

/**
 * @brief Finish handling the panic, clean everything up and halt.
 * @note Does not return
 */
void arch_panic_finalize() {
    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);
}

/**
 * @brief Returns the current CPU active in the system
 */
int arch_current_cpu() {
    return 0;
}

/**
 * @brief Get the generic parameters
 */
generic_parameters_t *arch_get_generic_parameters() {
    return parameters;
}


extern uintptr_t __bss_end;
static uintptr_t highest_kernel_address = ((uintptr_t)&__bss_end);  // This is ONLY used until memory management is initialized.
                                                                    // mm will take over this
static uintptr_t memory_size = 0x0;                                 // Same as above

/**
 * @brief Zeroes and allocates bytes for a structure at the end of the kernel
 * @param bytes Amount of bytes to allocate
 * @returns The address to which the structure can be placed at 
 */
uintptr_t arch_allocate_structure(size_t bytes) {
    dprintf(DEBUG, "Reserving 0x%x - 0x%x\n", highest_kernel_address, highest_kernel_address + bytes);
    uintptr_t ptr = highest_kernel_address;
    highest_kernel_address += bytes;

    memset((void*)ptr, 0x00, bytes);
    return ptr;
}

/**
 * @brief Copy & relocate a structure to the end of the kernel.
 * @param structure_ptr A pointer to the structure
 * @param size The size of the structure
 * @returns The address to which it was relocated.
 */
uintptr_t arch_relocate_structure(uintptr_t structure_ptr, size_t size) {
    uintptr_t location = arch_allocate_structure(size);
    memcpy((void*)location, (void*)structure_ptr, size);
    return location;
}


/**
 * @brief Main architecture function
 */
void arch_main(multiboot_t *bootinfo, uint32_t multiboot_magic, void *esp) {
    // !!!: Relocations may be required if I ever add back the relocatable tag
    // !!!: (which I should for compatibility)

    // Initialize the hardware abstraction layer
    hal_init(HAL_STAGE_1);

    // Align kernel address
    highest_kernel_address += PAGE_SIZE;
    highest_kernel_address &= ~0xFFF;

    // Parse Multiboot information
    if (multiboot_magic == MULTIBOOT_MAGIC) {
        dprintf(INFO, "Found a Multiboot1 structure\n");
        arch_parse_multiboot1_early(bootinfo, &memory_size, &highest_kernel_address);
    } else if (multiboot_magic == MULTIBOOT2_MAGIC) {
        dprintf(INFO, "Found a Multiboot2 structure\n");
        arch_parse_multiboot2_early(bootinfo, &memory_size, &highest_kernel_address);
    } else {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** Unknown multiboot structure when checking kernel.\n");
    }

    


    for (;;);
}