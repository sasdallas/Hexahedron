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

// Architecture-specific
#include <kernel/arch/x86_64/arch.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/mem.h>

// General
#include <kernel/kernel.h>
#include <kernel/config.h>
#include <kernel/multiboot.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/generic_mboot.h>
#include <kernel/processor_data.h>

// Memory
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/pmm.h>

// Misc
#include <kernel/misc/spinlock.h>
#include <kernel/misc/args.h>
#include <kernel/misc/ksym.h>

// Graphics
#include <kernel/gfx/gfx.h>

// Filesystem
#include <kernel/fs/vfs.h>
#include <kernel/fs/tarfs.h>
#include <kernel/fs/ramdev.h>

// Loader
#include <kernel/loader/driver.h>

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

        printf("%i system processors - %i KB of RAM\n", smp_getCPUCount(), parameters->mem_size);

        // NOTE: This should only be called once, so why not just modify some parameters?
        parameters->cpu_count = smp_getCPUCount();

        // Draw logo
        gfx_drawLogo(RGB(255, 255, 255));
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
 * @brief Perform a stack trace using ksym
 * @param depth How far back to stacktrace
 * @param registers Optional registers
 */
void arch_panic_traceback(int depth, registers_t *regs) {
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "\nSTACK TRACE:\n");

extern uintptr_t __bss_end;
    stack_frame_t *stk = (stack_frame_t*)(regs ? (void*)regs->rbp : __builtin_frame_address(0));
    uintptr_t ip = (regs ? regs->rip : (uintptr_t)&arch_panic_traceback);

    for (int frame = 0; stk && frame < depth; frame++) {
        // Check to see if fault was in a driver
        if (ip > MEM_DRIVER_REGION && ip < MEM_DRIVER_REGION + MEM_DRIVER_REGION_SIZE) {
            // Fault in a driver - try to get it
            loaded_driver_t *data = driver_findByAddress(ip);
            if (data) {
                // We could get it
                dprintf(NOHEADER, COLOR_CODE_RED    "0x%016llX (in driver '%s', loaded at %016llX)\n", ip, data->metadata->name, data->load_address);
            } else {
                // We could not
                dprintf(NOHEADER, COLOR_CODE_RED    "0x%016llX (in unknown driver)\n", ip);
            }

            goto _next_frame;
        }

        // Okay, make sure it was in the kernel
        if (ip > (uintptr_t)&__bss_end) {
            // Corrupt frame?
            dprintf(NOHEADER, COLOR_CODE_RED    "0x%016llX (corrupt frame - outside of kernelspace)\n", ip);
            goto _next_frame;
        }
        
        // In the kernel, check the name
        char *name;
        uintptr_t addr = ksym_find_best_symbol(ip, (char**)&name);
        if (addr) {
            dprintf(NOHEADER, COLOR_CODE_RED    "0x%016llX (%s+0x%llX)\n", ip, name, ip - addr);
        } else {
            dprintf(NOHEADER, COLOR_CODE_RED    "0x%016llX (symbols unavailable)\n", ip);
        }

    _next_frame:
        ip = stk->ip;
        stk = stk->nextframe;
    }
}

/**
 * @brief Prepare the architecture to enter a fatal state. This means cleaning up registers,
 * moving things around, whatever you need to do
 */
void arch_panic_prepare() {
    dprintf(ERR, "Fatal panic state detected - please wait, cleaning up...\n");
    smp_disableCores();
}

/**
 * @brief Finish handling the panic, clean everything up and halt.
 * @note Does not return
 */
void arch_panic_finalize() {
    // Perform a traceback
    arch_panic_traceback(10, NULL);

    // Display message
    dprintf(NOHEADER, COLOR_CODE_RED "\nThe kernel will now permanently halt. Connect a debugger for more information.\n");

    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);
}

/**** INTERNAL ARCHITECTURE FUNCTIONS ****/


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
    return (uintptr_t)kmalloc(bytes);
}

/**
 * @brief Copy & relocate a structure to the end of the kernel.
 * @param structure_ptr A pointer to the structure
 * @param size The size of the structure
 * @returns The address to which it was relocated.
 */
uintptr_t arch_relocate_structure(uintptr_t structure_ptr, size_t size) {
    uintptr_t location = arch_allocate_structure(size);
    memcpy((void*)location, (void*)mem_remapPhys(structure_ptr, size), size);
    return location;
}

/**
 * @brief Set the GSbase using MSRs
 */
void arch_set_gsbase(uintptr_t base) {
    cpu_setMSR(X86_64_MSR_GSBASE, base & 0xFFFFFFFF, (base >> 32) & 0xFFFFFFFF);
    cpu_setMSR(X86_64_MSR_KERNELGSBASE, base & 0xFFFFFFFF, (base >> 32) & 0xFFFFFFFF);
    asm volatile ("swapgs");
}

/**
 * @brief Main architecture function
 */
void arch_main(multiboot_t *bootinfo, uint32_t multiboot_magic, void *esp) {
    // !!!: Relocations may be required if I ever add back the relocatable tag
    // !!!: (which I should for compatibility)

    // Setup GSBase first
    arch_set_gsbase((uintptr_t)&processor_data[0]);

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

    // Now, we can initialize memory systems.
    mem_init(memory_size, highest_kernel_address);

    // Print out allocator information
    allocator_info_t *info = alloc_getInfo();
    dprintf(INFO, "Allocator information: %s version %i.%i (valloc %s, profiling %s)\n", info->name, info->version_major, info->version_minor,
                                            info->support_valloc ? "supported" : "not supported",
                                            info->support_profile ? "supported" : "not supported");

    // Now we can ACTUALLY parse Multiboot information
    if (multiboot_magic == MULTIBOOT_MAGIC) {
        parameters = arch_parse_multiboot1(bootinfo);
    } else if (multiboot_magic == MULTIBOOT2_MAGIC) {
        parameters = arch_parse_multiboot2(bootinfo);
    }

    dprintf(INFO, "Loaded by '%s' with command line '%s'\n", parameters->bootloader_name, parameters->kernel_cmdline);
    dprintf(INFO, "Available physical memory to machine: %i KB\n", parameters->mem_size);

    // Initialize arguments system
    kargs_init(parameters->kernel_cmdline);

    // We're clear to perform the second part of HAL startup
    hal_init(HAL_STAGE_2); 

    // All done. Jump to kernel main.
    kmain();

    for (;;);
}