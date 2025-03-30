/**
 * @file libpolyhedron/stdio/fileio.c
 * @brief Internal file I/O operations
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Write file bytes
 * @param f The file object to write to
 * @param buf The buffer to write
 * @param size The size of the buffer to write
 */
size_t __fileio_write_bytes(FILE *f, char *buf, size_t size) {
    if (!f) return 0;

    // Allocate a write buffer if needed
    if (!f->wbuf) {
        f->wbuf = malloc(WRITE_BUFFER_SIZE);
        f->wbufsz = WRITE_BUFFER_SIZE;
    }

    // Now let's sequentially write data to the file
    for (size_t i = 0; i < size; i++) {
        f->wbuf[f->wbuflen++] = buf[i];

        // Do we need to flush?
        if (f->wbuflen >= f->wbufsz || (buf[i] == '\n')) {
            fflush(f);
        }
    }

    return size;
}