/**
 * @file hexahedron/arch/i386/smp.c
 * @brief Symmetric multiprocessor handler
 * 
 * The joys of synchronization primitives are finally here.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/i386/smp.h>
#include <kernel/arch/i386/hal.h>
#include <kernel/arch/i386/interrupt.h>
#include <kernel/arch/i386/cpu.h>
#include <kernel/processor_data.h>
#include <kernel/drivers/x86/local_apic.h>
#include <kernel/drivers/x86/clock.h>


#include <kernel/mem/pmm.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/gfx/term.h>

#include <string.h>
#include <stddef.h>
#include <errno.h>

/* SMP data */
static smp_info_t *smp_data = NULL;

/* CPU data */
processor_t processor_data[MAX_CPUS] = {0};

/* CPU count */
int processor_count = 1;



/* Local APIC mmio address */
uintptr_t lapic_remapped = 0;

/* Remapped page for the bootstrap code */
static uintptr_t bootstrap_page_remap = 0;

/* Core stack - this is used after paging is setup */
uint32_t _ap_stack_base = 0;

/* Trampoline variables */
extern uint32_t _ap_bootstrap_start, _ap_bootstrap_end;

/* AP startup flag. This will change when the AP finishes starting */
static int ap_startup_finished = 0;

/* AP shutdown flag. This will change when the AP finishes shutting down. */
static int ap_shutdown_finished = 0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SMP", __VA_ARGS__)


/**
 * @brief Sleep for a short period of time
 * @param delay How long to sleep for
 */
static void smp_delay(unsigned int delay) {
    uint64_t clock = clock_readTSC();
    while (clock_readTSC() < clock + delay * clock_getTSCSpeed());
}

/**
 * @brief Collect AP information to store in processor_data
 * @param ap The core to store information on
 */
static void smp_collectAPInfo(int ap) {
    current_cpu->cpu_id = smp_getCurrentCPU();
    current_cpu->cpu_manufacturer = cpu_getVendorName();
    strncpy(current_cpu->cpu_model, cpu_getBrandString(), 48);
    current_cpu->cpu_model_number = cpu_getModelNumber();
    current_cpu->cpu_family = cpu_getFamily();
}


/**
 * @brief Finish an AP's setup. This is done right after the trampoline code gets to 32-bit mode and sets up a stack
 * @param params The AP parameters set up by @c smp_prepareAP
 */
__attribute__((noreturn)) void smp_finalizeAP() {
    // We want all cores to have a consistent GDT
    extern void hal_installGDT();
    hal_installGDT();
    
    // Install the IDT
    hal_installIDT();

    // Setup paging for this AP
    mem_switchDirectory(mem_getKernelDirectory());
    mem_setPaging(true);

    // HACK: We must load the stack here after paging has initialized. Trampoline will load a temporary stack.
    asm volatile ("movl %0, %%esp" :: "m"(_ap_stack_base));

    // Reinitialize the APIC
    lapic_initialize(lapic_remapped);

    // Now collect information
    smp_collectAPInfo(smp_getCurrentCPU());

    // Allow BSP to continue
    LOG(DEBUG, "CPU%i online and ready\n", smp_getCurrentCPU());
    ap_startup_finished = 1;

    for (;;);
}




/**
 * @brief Start an AP
 * @param lapic_id The ID of the local APIC to start
 */
void smp_startAP(uint8_t lapic_id) {
    ap_startup_finished = 0;

    // Copy the bootstrap code. The AP might've messed with it.
    memcpy((void*)bootstrap_page_remap, (void*)&_ap_bootstrap_start, (uintptr_t)&_ap_bootstrap_end - (uintptr_t)&_ap_bootstrap_start);

    // Allocate a stack for the AP
    if (alloc_canHasValloc()) {
        _ap_stack_base = (uint32_t)kvalloc(PAGE_SIZE) + PAGE_SIZE;
    } else {
        _ap_stack_base = (uint32_t)mem_sbrk(PAGE_SIZE * 2) + (PAGE_SIZE);       // !!!: Giving two pages when we're only using one 
                                                                                // !!!: Stack alignment issues - you can also use kvalloc but some allocators don't support it in Hexahedron
    }

    memset((void*)(_ap_stack_base - 4096), 0, PAGE_SIZE);

    // Send the INIT signal
    lapic_sendInit(lapic_id);
    smp_delay(5000UL);

    // Send SIPI
    lapic_sendStartup(lapic_id, SMP_AP_BOOTSTRAP_PAGE);

    // Wait for AP to finish
    do { asm volatile ("pause" ::: "memory"); } while (!ap_startup_finished);
}

/**
 * @brief Initialize the SMP system
 * @param info Collected SMP information
 * @returns 0 on success, non-zero is failure
 */
int smp_init(smp_info_t *info) {
    if (!info) return -EINVAL;
    smp_data = info;

    // Local APIC region is finite size - at least I hope.
    lapic_remapped = mem_mapMMIO((uintptr_t)smp_data->lapic_address, PAGE_SIZE);

    // Initialize the local APIC
    int lapic = lapic_initialize(lapic_remapped);
    if (lapic != 0) {
        LOG(ERR, "Failed to initialize local APIC");
        return -EIO;
    }

    // The AP expects its code to be bootstrapped to a page-aligned address (SIPI expects a starting page number)
    // The remapped page for SMP is stored in the variable SMP_AP_BOOTSTRAP_PAGE
    // Assuming that page has some content in it, copy and store it.
    // !!!: This is a bit hacky as we're playing with fire here. What if PMM_BLOCK_SIZE != PAGE_SIZE?
    uintptr_t temp_frame = pmm_allocateBlock();
    uintptr_t temp_frame_remap = mem_remapPhys(temp_frame, PAGE_SIZE);
    bootstrap_page_remap = mem_remapPhys(SMP_AP_BOOTSTRAP_PAGE, PAGE_SIZE);
    memcpy((void*)temp_frame_remap, (void*)bootstrap_page_remap, PAGE_SIZE);

    // Start APs
    // WARNING: Starting CPU0/BSP will triple fault (bad)
    for (int i = 0; i < smp_data->processor_count; i++) {
        if (i != 0) {
            smp_startAP(smp_data->lapic_ids[i]);
        }
    }

    // Finished! Unmap bootstrap code
    memcpy((void*)bootstrap_page_remap, (void*)temp_frame_remap, PAGE_SIZE);
    mem_unmapPhys(temp_frame_remap, PAGE_SIZE);
    mem_unmapPhys(bootstrap_page_remap, PAGE_SIZE);
    pmm_freeBlock(temp_frame);

    processor_count = smp_data->processor_count;
    LOG(INFO, "SMP initialization completed successfully - %i CPUs available to system\n", processor_count);

    return 0;
}

/**
 * @brief Get the amount of CPUs present in the system
 */
int smp_getCPUCount() {
    return processor_count;
}

/**
 * @brief Get the current CPU's APIC ID
 */
int smp_getCurrentCPU() {
	uint32_t ebx, unused;
    __cpuid(0x1, unused, ebx, unused, unused);
    return (int)(ebx >> 24);
}

/**
 * @brief Acknowledge core shutdown (called by ISR)
 * 
 * @bug On an NMI, we just assume it's a core shutdown - is this okay?
 */
void smp_acknowledgeCoreShutdown() {
    LOG(INFO, "CPU%i finished shutting down\n", smp_getCurrentCPU());
    ap_shutdown_finished = 1;
}

/**
 * @brief Shutdown all cores in a system
 * 
 * This causes ISR2 (NMI) to be thrown, disabling the core's interrupts and 
 * looping it on a halt instruction.
 */
void smp_disableCores() {
    if (smp_data == NULL) return;
    LOG(INFO, "Disabling cores - please wait...\n");

    for (int i = 0; i < smp_data->processor_count; i++) {
        if (i != 0) {
            lapic_sendNMI(smp_data->lapic_ids[i], 124);

            uint8_t error;
            do { asm volatile ("pause"); } while (!ap_shutdown_finished && !(error = lapic_readError()));

            if (error) {
                LOG(WARN, "APIC error detected while shutting down CPU%i: ESR read as 0x%x\n", i, error);
                LOG(WARN, "Failed to shutdown SMP cores. Continuing anyway.\n");
                break;
            }

            ap_shutdown_finished = 0;
        }
    }
}