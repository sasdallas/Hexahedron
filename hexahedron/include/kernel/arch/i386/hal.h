/**
 * @file hexahedron/include/kernel/arch/i386/hal.h
 * @brief Architecture-specific HAL functions
 * 
 * HAL functions that need to be called from other parts of the architecture (e.g. hardware-specific drivers)
 * are implemented here.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_I386_HAL_H
#define KERNEL_ARCH_I386_HAL_H

/**** INCLUDES ****/
#include <kernel/arch/i386/interrupt.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define HAL_STAGE_1     1   // Stage 1 of HAL initialization
#define HAL_STAGE_2     2   // Stage 2 of HAL initialization

/**** FUNCTIONS ****/
 
/**
 * @brief Initialize the hardware abstraction layer
 * 
 * Initializes serial output, memory systems, and much more for I386.
 *
 * @param stage What stage of HAL initialization should be performed.
 *              Specify HAL_STAGE_1 for initial startup, and specify HAL_STAGE_2 for post-memory initialization startup.
 * @todo A better driver interface is needed.
 */
void hal_init(int stage);

/**
 * @brief Initialize HAL interrupts (IDT, GDT, TSS, etc.)
 */
void hal_initializeInterrupts();

/**
 * @brief Initialize the 8259 PIC(s)
 * Uses default offsets 0x20 for master and 0x28 for slave
 */
void hal_initializePIC();

/**
 * @brief Disable the 8259 PIC(s)
 */
void hal_disablePIC();

/**
 * @brief Installs the IDT in the current AP
 */
void hal_installIDT();

/**
 * @brief Register an interrupt handler
 * @param int_no Interrupt number (start at 0)
 * @param handler A handler. This should return 0 on success, anything else panics.
 *                It will take registers and extended registers as arguments.
 * @returns 0 on success, -EINVAL if handler is taken
 */
int hal_registerInterruptHandler(uintptr_t int_no, interrupt_handler_t handler);

/**
 * @brief Unregisters an interrupt handler
 */
void hal_unregisterInterruptHandler(uintptr_t int_no);

/**
 * @brief Register an exception handler
 * @param int_no Exception number
 * @param handler A handler. This should return 0 on success, anything else panics.
 *                It will take an exception number, registers, and extended registers as arguments.
 * @returns 0 on success, -EINVAL if handler is taken
 */
int hal_registerExceptionHandler(uintptr_t int_no, exception_handler_t handler);

/**
 * @brief Unregisters an exception handler
 */
void hal_unregisterExceptionHandler(uintptr_t int_no);

/**
 * @brief Sets an RSDP if one was set
 */
void hal_setRSDP(uint64_t rsdp);

/**
 * @brief Returns a RSDP if one was found
 */
uint64_t hal_getRSDP();

/* I/O port functions (no headers) */
void io_wait();
void outportb(unsigned short port, unsigned char data);
void outportw(unsigned short port, unsigned short data);
void outportl(unsigned short port, unsigned long data);
unsigned char inportb(unsigned short port);
unsigned short inportw(unsigned short port);
unsigned long inportl(unsigned short port);



#endif