/**
 * @file libpolyhedron/arch/x86_64/time/now.c
 * @brief now() function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <time.h>
#include <sys/time.h>


unsigned long long now() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec;
}