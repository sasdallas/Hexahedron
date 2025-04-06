/**
 * @file hexahedron/task/sleep.c
 * @brief Thread blocker/sleeper handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>
#include <kernel/task/sleep.h>
#include <kernel/mem/alloc.h>
#include <structs/list.h>
#include <kernel/debug.h>
#include <string.h>

/* Sleeping queue */
list_t *sleep_queue = NULL;

/* Sleep queue lock */
spinlock_t sleep_queue_lock = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SLEEP", __VA_ARGS__)

/**
 * @brief Wakeup sleepers callback
 * @param ticks Current clock ticks
 */
static void sleep_callback(uint64_t ticks) {
    if (!sleep_queue) return;

    // Get time for any threads that need it
    unsigned long seconds, subseconds;
    clock_getCurrentTime(&seconds, &subseconds);
    
    spinlock_acquire(&sleep_queue_lock);
    foreach(node, sleep_queue) {
        thread_sleep_t *sleep = (thread_sleep_t*)node->value;
        if (sleep->sleep_state == SLEEP_FLAG_NOCOND) continue;
        if (sleep->sleep_state == SLEEP_FLAG_TIME) {
            // We're sleeping on time.
            if ((sleep->seconds == seconds && sleep->subseconds <= subseconds) || (sleep->seconds < seconds)) {
                // Wakeup now
                list_delete(sleep_queue, node);
                kfree(node);

                __sync_and_and_fetch(&sleep->thread->status, ~(THREAD_STATUS_SLEEPING));
                sleep->thread->sleep = NULL;
                scheduler_insertThread(sleep->thread);
                kfree(sleep);
            }
        }
    }

    spinlock_release(&sleep_queue_lock);
}

/**
 * @brief Initialize the sleeper system
 */
void sleep_init() {
    sleep_queue = list_create("thread sleep queue");
    clock_registerUpdateCallback(sleep_callback);
}


/**
 * @brief Put a thread to sleep, no condition and no way to wakeup without @c sleep_wakeup
 * @param thread The thread to sleep
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, yield immediately after without rescheduling.
 */
int sleep_untilNever(struct thread *thread) {
    if (!thread) return 1;

    // Construct a sleep node
    thread_sleep_t *sleep = kmalloc(sizeof(thread_sleep_t));
    memset(sleep, 0, sizeof(thread_sleep_t));
    sleep->sleep_state = SLEEP_FLAG_NOCOND;
    sleep->thread = thread;
    thread->sleep = sleep;

    // Manually create node..
    node_t *node = kmalloc(sizeof(node_t));
    node->value = (void*)sleep;

    spinlock_acquire(&sleep_queue_lock);
    list_append_node(sleep_queue, node);
    spinlock_release(&sleep_queue_lock);

    // Mark thread as sleeping. If process_yield finds this thread to be trying to reschedule,
    // it will disallow it and just switch away
    __sync_or_and_fetch(&thread->status, THREAD_STATUS_SLEEPING);
    return 0;
}

/**
 * @brief Put a thread to sleep until a specific amount of time in the future has passed
 * @param thread The thread to put to sleep
 * @param seconds Seconds to wait in the future
 * @param subseconds Subseconds to wait in the future
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, yield immediately after without rescheduling.
 */
int sleep_untilTime(struct thread *thread, unsigned long seconds, unsigned long subseconds) {
    if (!thread) return 1;

    // Construct a sleep node
    thread_sleep_t *sleep = kmalloc(sizeof(thread_sleep_t));
    memset(sleep, 0, sizeof(thread_sleep_t));
    sleep->sleep_state = SLEEP_FLAG_TIME;
    sleep->thread = thread;
    thread->sleep = sleep;

    // Get relative time
    unsigned long new_seconds, new_subseconds;
    clock_relative(seconds, subseconds, &new_seconds, &new_subseconds);
    sleep->seconds = new_seconds;
    sleep->subseconds = new_subseconds;

    // Manually create node..
    node_t *node = kmalloc(sizeof(node_t));
    node->value = (void*)sleep;

    spinlock_acquire(&sleep_queue_lock);
    list_append_node(sleep_queue, node);
    spinlock_release(&sleep_queue_lock);

    // Mark thread as sleeping. If process_yield finds this thread to be trying to reschedule,
    // it will disallow it and just switch away
    __sync_or_and_fetch(&thread->status, THREAD_STATUS_SLEEPING);
    return 0;
}