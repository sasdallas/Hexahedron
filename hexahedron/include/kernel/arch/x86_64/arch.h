/**
 * @file hexahedron/include/kernel/arch/x86_64/arch.h
 * @brief x86_64 main architecture header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_ARCH_H
#define KERNEL_ARCH_X86_64_ARCH_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/generic_mboot.h>
#include <kernel/multiboot.h>
#include <kernel/arch/arch.h>
#include <kernel/arch/x86_64/registers.h>

/**** TYPES ****/

typedef struct _stack_frame {
    struct _stack_frame *nextframe;
    uintptr_t ip;
} stack_frame_t;

/**** FUNCTIONS ****/

/**
 * @brief Say hi! Prints the versioning message and ASCII art to NOHEADER dprintf
 * @param is_debug Print to regular printf or dprintf
 */
void arch_say_hello(int is_debug);

/**
 * @brief Parse a Multiboot 1 header and packs into a @c generic_parameters structure
 * @param bootinfo The Multiboot information
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot1(multiboot_t *bootinfo);

/** 
 * @brief Parse a Multiboot 2 header and packs into a @c generic_parameters structure
 * @param bootinfo A pointer to the multiboot informatino
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot2(multiboot_t *bootinfo);

/**
 * @brief x86_64-specific parser function for Multiboot1
 * @param bootinfo The boot information
 * @param mem_size Output pointer to mem_size
 * @param kernel_address Output pointer to highest valid kernel address
 * 
 * This is here because paging is already enabled in x86_64, meaning
 * we have to initialize the allocator. It's very hacky, but it does end up working.
 * (else it will overwrite its own page tables and crash or something, i didn't do much debugging)
 */
void arch_parse_multiboot1_early(multiboot_t *bootinfo, uintptr_t *mem_size, uintptr_t *kernel_address);

/**
 * @brief x86_64-specific parser function for Multiboot2
 * @param bootinfo The boot information
 * @param mem_size Output pointer to mem_size
 * @param kernel_address Output pointer to highest valid kernel address
 * 
 * This is here because paging is already enabled in x86_64, meaning
 * we have to initialize the allocator. It's very hacky, but it does end up working.
 * (else it will overwrite its own page tables and crash or something, i didn't do much debugging)
 */
void arch_parse_multiboot2_early(multiboot_t *bootinfo, uintptr_t *mem_size, uintptr_t *kernel_address);


/**
 * @brief Mark/unmark valid spots in memory
 * @todo Work in tandem with mem.h to allow for a maximum amount of blocks to be used
 */
void arch_mark_memory(generic_parameters_t *parameters, uintptr_t highest_address);

/**
 * @brief Perform a stack trace using ksym
 * @param depth How far back to stacktrace
 * @param registers Optional registers
 */
void arch_panic_traceback(int depth, registers_t *regs);

/**
 * @brief Set the GSbase using MSRs
 */
void arch_set_gsbase(uintptr_t base);

#endif