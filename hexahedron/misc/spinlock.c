/**
 * @file hexahedron/misc/spinlock.c
 * @brief Spinlock implementation
 * 
 * This is used for safeguarding SMP memory accesses. It is an internal spinlock implementation.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/misc/spinlock.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <stdatomic.h>

/**
 * @brief Create a new spinlock
 * @param name Optional name parameter
 * @returns A new spinlock structure
 */
spinlock_t *spinlock_create(char *name) {
    spinlock_t *ret = kmalloc(sizeof(spinlock_t));
    ret->cpu = 0; // TODO: SMP
    ret->name = name;
    
    // Because atomics and its consequences have been a disaster for the human race,
    // we must use atomic_flag_clear to set this variable.
    // https://stackoverflow.com/questions/31526556/initializing-an-atomic-flag-in-a-mallocd-structure
    atomic_flag_clear(&(ret->lock));

    return ret;
}

/**
 * @brief Destroys a spinlock
 * @param spinlock Spinlock to destroy
 */
void spinlock_destroy(spinlock_t *spinlock) {
    kfree(spinlock); // TODO: Atomics?
}

/**
 * @brief Lock a spinlock
 * 
 * Will spin around until we acquire the lock
 */
void spinlock_acquire(spinlock_t *spinlock) {
    while (atomic_flag_test_and_set_explicit(&(spinlock->lock), memory_order_acquire)) {
        // TODO: CPU pause
    }
}

/**
 * @brief Release a spinlock
 */
void spinlock_release(spinlock_t *spinlock) {
    atomic_flag_clear_explicit(&(spinlock->lock), memory_order_release);
}
