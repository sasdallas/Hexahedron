/**
 * @file libpolyhedron/unistd/exit.c
 * @brief exit
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>

DEFINE_SYSCALL1(exit, SYS_EXIT, int);

void exit(int status) {
    // TODO: Actually properly exit
    __syscall_exit(status);
}