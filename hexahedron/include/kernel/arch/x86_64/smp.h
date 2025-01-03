/**
 * @file hexahedron/include/kernel/arch/x86_64/smp.h
 * @brief Symmetric multiprocessing header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_SMP_H
#define KERNEL_ARCH_X86_64_SMP_H

/**** INCLUDES ****/
#include <stdint.h>


/**** DEFINITIONS ****/

// IMPORTANT: This value controls the maximum amount of CPUs Hexahedron supports
#define MAX_CPUS            32

// IMPORTANT: This value controls the maximum amount of supported interrupt overrides
#define MAX_INT_OVERRIDES   24


/**** TYPES ****/

// Structure passed to SMP driver containing information (MADT/MP table/whatever)
typedef struct _smp_info {
    // CPUs/local APICs
    void        *lapic_address;             // Local APIC address (will be mmio'd)
    uint8_t     processor_count;            // Amount of processors
    uint8_t     processor_ids[MAX_CPUS];    // Local APIC processor IDs
    uint8_t     lapic_ids[MAX_CPUS];        // Local APIC IDs

    // I/O APICs
    uint16_t    ioapic_count;               // I/O APICs
    uint8_t     ioapic_ids[MAX_CPUS];       // I/O APIC IDs
    uint32_t    ioapic_addrs[MAX_CPUS];     // I/O APIC addresses
    uint32_t    ioapic_irqbases[MAX_CPUS];  // I/O APIC global IRQ bases
    
    // Overrides
    uint32_t    irq_overrides[MAX_INT_OVERRIDES];   // IRQ overrides (index of array = source, content = map)
} smp_info_t;

typedef struct _smp_ap_parameters {
    uint32_t stack;
    uint32_t idt;
    uint32_t pagedir;
    uint32_t lapic_id;
} smp_ap_parameters_t;

#endif