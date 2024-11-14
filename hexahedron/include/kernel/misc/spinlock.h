/**
 * @file hexahedron/include/kernel/misc/spinlock.h
 * @brief Spinlock implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MISC_SPINLOCK_H
#define KERNEL_MISC_SPINLOCK_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdatomic.h>

/**** TYPES ****/

// spin
typedef struct spinlock {
    char *name;         // Optional name
    int cpu;            // ID of the CPU holding the spinlock 
    atomic_flag lock;   // Lock
} spinlock_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new spinlock
 * @param name Optional name parameter
 * @returns A new spinlock structure
 */
spinlock_t *spinlock_create(char *name);

/**
 * @brief Destroys a spinlock
 * @param spinlock Spinlock to destroy
 */
void spinlock_destroy(spinlock_t *spinlock);

/**
 * @brief Lock a spinlock
 * 
 * Will spin around until we acquire the lock
 */
void spinlock_acquire(spinlock_t *spinlock);

/**
 * @brief Release a spinlock
 */
void spinlock_release(spinlock_t *spinlock);


#endif