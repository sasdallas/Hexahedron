/**
 * @file hexahedron/include/kernel/task/block.h
 * @brief Thread blocking/sleeping 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_BLOCK_H
#define KERNEL_TASK_BLOCK_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/mem.h>
#include <structs/node.h>

/**** DEFINITIONS ****/

/* Internal sleeping flags */
#define SLEEP_FLAG_NOCOND           0       // There is no condition under which the thread should wakeup. Dead thread. Should only be used for debugging.
#define SLEEP_FLAG_WAKEUP           1       // Whatever the case, wake it up NOW!
#define SLEEP_FLAG_TIME             2       // Thread is sleeping on time.
#define SLEEP_FLAG_COND             3       // Sleeping on condition

/**** TYPES ****/

/**
 * @brief Sleep condition function
 * @param context Context provided by @c thread_blockXXX
 * @returns 0 on not ready to resume, 1 on ready to resume
 */
typedef int (*sleep_condition_t)(void *context);

/**
 * @brief Sleeper structure
 */
typedef struct thread_sleep {
    struct thread *thread;                  // Thread which is sleeping
    node_t *node;                           // Assigned node in the sleeping queue
    volatile int sleep_state;               // Sleeping flags

    // Conditional-sleeping threads
    sleep_condition_t condition;            // Condition on which to wakeup
    void *context;                          // Context for said condition

    // Specific to threads sleeping for time
    unsigned long seconds;                  // Seconds on which to wakeup
    unsigned long subseconds;               // Subseconds on which to wakeup
} thread_sleep_t;


/**** FUNCTIONS ****/

/**
 * @brief Initialize the sleeper system
 */
void sleep_init();

/**
 * @brief Put a thread to sleep, no condition and no way to wakeup without @c sleep_wakeup
 * @param thread The thread to sleep
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, yield immediately after without rescheduling.
 */
int sleep_untilNever(struct thread *thread);

/**
 * @brief Put a thread to sleep until a specific condition is ready
 * @param thread The thread to put to sleep
 * @param condition Condition function
 * @param context Optional context passed to condition function
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, yield immediately after without rescheduling.
 */
int sleep_untilCondition(struct thread *thread, sleep_condition_t condition, void *context);

/**
 * @brief Put a thread to sleep until a specific amount of time in the future has passed
 * @param thread The thread to put to sleep
 * @param seconds Seconds to wait in the future
 * @param subseconds Subseconds to wait in the future
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, yield immediately after without rescheduling.
 */
int sleep_untilTime(struct thread *thread, unsigned long seconds, unsigned long subseconds);

/**
 * @brief Put a thread to sleep until a spinlock has unlocked
 * @param thread The thread to put to sleep
 * @param lock The lock to wait on
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, yield immediately after without rescheduling.
 */
int sleep_untilUnlocked(struct thread *thread, spinlock_t *lock);

/**
 * @brief Immediately trigger an early wakeup on a thread
 * @param thread The thread to wake up
 * @returns 0 on success
 */
int sleep_wakeup(struct thread *thread);

#endif