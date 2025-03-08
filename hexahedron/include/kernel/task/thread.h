/**
 * @file hexahedron/include/kernel/task/thread.h
 * @brief Thread file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_THREAD_H
#define KERNEL_TASK_THREAD_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/mem/mem.h>

/**** DEFINITIONS ****/

// Thread status flags
#define THREAD_STATUS_KERNEL        0x01
#define THREAD_STATUS_STOPPED       0x02
#define THREAD_STATUS_RUNNING       0x04
#define THREAD_STATUS_SLEEPING      0x08
#define THREAD_STATUS_STOPPING      0x10

// Thread flags
#define THREAD_FLAG_DEFAULT         0x00
#define THREAD_FLAG_KERNEL          0x01
#define THREAD_FLAG_NO_PREEMPT      0x02    // This only works on threads with THREAD_FLAG_KERNEL

// Stack size of thread
#define THREAD_STACK_SIZE           4096

/**** TYPES ****/

// Prototype
struct process;

/**
 * @brief Thread structure. Contains an execution path in a process.
 */
typedef struct thread {
    // GENERAL VARIABLES
    struct process *parent;     // Parent process
    unsigned int status;        // Status of this thread
    unsigned int flags;         // Flags of the thread

    // SCHEDULER TIMES
    time_t preempt_ticks;       // Ticks until the thread is preempted
    time_t total_ticks;         // Total amount of ticks the thread has been running for
    time_t start_ticks;         // Starting ticks

    // THREAD VARIABLES
    arch_context_t context;     // Thread context (defined by architecture)
    uint8_t fp_regs[512] __attribute__((aligned(16))); // FPU registers (TEMPORARY - should be moved into arch_context?)

    page_t *dir;                // Page directory for the thread
    uintptr_t stack;            // Thread stack (kernel will load parent->kstack in TSS)
} thread_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new thread
 * @param parent The parent process of the thread
 * @param dir Directory to use (for new threads being used as main, mem_clone() this first, else refcount the main thread's directory)
 * @param entrypoint The entrypoint of the thread (you can also set this later)
 * @param flags Flags of the thread
 * @returns New thread pointer, just save context & add to scheduler queue
 */
thread_t *thread_create(struct process *parent, page_t *dir, uintptr_t entrypoint, int flags);

/**
 * @brief Destroys a thread. ONLY CALL ONCE THE THREAD IS FULLY READY TO BE DESTROYED
 * @param thr The thread to destroy
 */
int thread_destroy(thread_t *thr);

#endif