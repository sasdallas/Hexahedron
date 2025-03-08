/**
 * @file hexahedron/include/kernel/processor_data.h
 * @brief Implements a generic and architecture-specific CPU core data system
 * 
 * The architectures that implement SMP need to update this structure with their own 
 * data fields. Generic data fields (like the current process, cpu_id, etc.) are required as well. 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_PROCESSOR_DATA_H
#define KERNEL_PROCESSOR_DATA_H

#pragma GCC diagnostic ignored "-Wunused-variable"

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/mem/mem.h>
#include <kernel/task/process.h>


/**** TYPES ****/

// Temporary prototypes because this and process file are an include mess
struct process;
struct thread;

typedef struct _processor {
    int cpu_id;                         // CPU ID
    page_t *current_dir;                // Current page directory
    struct thread *current_thread;      // Current thread of the process

    struct process *current_process;    // Current process of the CPU
                                        // TODO: Find a way to better organize tasking systems so this isn't needed, as thread should contain a pointer to this.
                                        // TODO: Mainly this is used because init shouldn't HAVE a thread, so process_execute can just use this

    struct process *idle_process;       // Idle process of the CPU
                                        // TODO: Maybe use thread instead of storing pointer to process structure

#if defined(__ARCH_X86_64__) || defined(__ARCH_I386__)

    // Another hack sourced from Toaru
#ifdef __ARCH_X86_64__
    uintptr_t kstack;                   // (0x40) Kernel-mode stack loaded in TSS
    uintptr_t ustack;                   // (0x48) Usermode stack, saved in SYSCALL entrypoint    
#endif

    int lapic_id;

    /* CPU basic information */
    char cpu_model[48];
    const char *cpu_manufacturer;
    int cpu_model_number;
    int cpu_family;
#endif
    

} processor_t;

/* External variables defined by architecture */
extern processor_t processor_data[];
extern int processor_count;

/**
 * @brief Architecture-specific method of determining current core
 * 
 * i386: We use a macro that retrieves the current data from processor_data
 * x86_64: We use the GSbase to get it
 */

#if defined(__ARCH_I386__)

#define current_cpu ((processor_t*)&(processor_data[arch_current_cpu()]))

#elif defined(__ARCH_X86_64__)

static processor_t __seg_gs * const current_cpu = 0;

#else
#error "Please define a method of getting processor data"
#endif


#endif