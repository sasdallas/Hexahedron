/**
 * @file libpolyhedron/include/sys/time.h
 * @brief 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_TIME_H
#define _SYS_TIME_H


/**** INCLUDES ****/
#include <sys/types.h>

/**** TYPES ****/

// Seconds/microseconds timeval
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

// Timezone
struct timezone {
    int tz_minuteswest; // Minutes west of Greenwich
    int tz_dsttime;     // Type of Daylight Savings Time correction
};

/**** FUNCTIONS ****/
extern int gettimeofday(struct timeval *ptr, void *z);
extern int settimeofday(struct timeval *ptr, void *z);

#endif

_End_C_Header