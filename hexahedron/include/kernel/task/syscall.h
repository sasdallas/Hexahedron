/**
 * @file hexahedron/include/kernel/task/syscall.h
 * @brief System call handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_SYSCALL_H
#define KERNEL_TASK_SYSCALL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>

/**** DEFINITIONS ****/

#define SYSCALL_MAX_PARAMETERS      6       // !!!: AT THE CURRENT TIME, ONLY 5 ARE SUPPORTED

/**** TYPES ****/

/**
 * @brief System call structure
 */
typedef struct syscall {
    int syscall_number;
    long parameters[SYSCALL_MAX_PARAMETERS];
    long return_value;
} syscall_t;

/**
 * @brief System call function
 * 
 * @warning We're treading in unknown waters here - overloading functions
 */
typedef long (*syscall_func_t)(long, long, long, long, long);

/**** FUNCTIONS ****/

/**
 * @brief Handle a system call
 * @param syscall The system call to handle
 * @returns Nothing, but updates @c syscall->return_value
 */
void syscall_handle(syscall_t *syscall);

/* System calls */
void sys_exit(int status);
int sys_open(const char *pathname, int flags, mode_t mode);
ssize_t sys_read(int fd, void *buffer, size_t count);
ssize_t sys_write(int fd, const void *buffer, size_t count);
int sys_close(int fd);
void *sys_brk(void *addr);

#endif