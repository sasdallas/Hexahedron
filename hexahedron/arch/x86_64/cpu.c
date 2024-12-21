/**
 * @file hexahedron/arch/x86_64/cpu.c
 * @brief x86_64 CPU interfacer
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/x86_64/cpu.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Checks whether the RDMSR/WRMSR instructions are supported
 */
int cpu_msrAvailable() {
    static uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    return edx & CPUID_FEAT_EDX_MSR;
}

/**
 * @brief Get a model-specific register
 * @param msr The MSR to get
 * @param lo The low bits (pointer output)
 * @param hi The high bits (pointer output)
 */
void cpu_getMSR(uint32_t msr, uint32_t *lo, uint32_t *hi) {
    if (!cpu_msrAvailable()) return;
    asm volatile ("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

/**
 * @brief Set a model-specific register
 * @param msr The MSR to set
 * @param lo The low bits to set with
 * @param hi The high bits to set with
 */
void cpu_setMSR(uint32_t msr, uint32_t lo, uint32_t hi) {
    if (!cpu_msrAvailable()) return;
    asm volatile ("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

/**
 * @brief Perform a CPUID instruction (only for strings)
 * 
 * Yes, I know of the existance of @c __cpuid but it doesn't fit my needs for model strings.
 */
static inline void cpuid(int code, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    asm volatile ("cpuid": 
                    "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) 
                    : "a"(code));
}

/**
 * @brief Get the vendor name of a CPU, cleaned up
 */
char *cpu_getVendorName() {
    char vendor[17];
    uint32_t unused;
    cpuid(0, &unused, (uint32_t*)&vendor, (uint32_t*)&(vendor[8]), (uint32_t*)&(vendor[4]));
    vendor[16] = 0;

    if (!strncmp(vendor, CPUID_VENDOR_AMD, 16)) {
        return "AMD";
    } else if (!strncmp(vendor, CPUID_VENDOR_INTEL, 16)) {
        return "Intel";
    }

    return "???";
}

/**
 * @brief Get the model number of a CPU
 */
uint8_t cpu_getModelNumber() {
    uint32_t eax, unused;
    __cpuid(0, eax, unused, unused, unused);
    return (eax >> 4) & 0x0F;
}

/**
 * @brief Get the family of a CPU
 */
uint8_t cpu_getFamily() {
    uint32_t eax, unused;
    __cpuid(0, eax, unused, unused, unused);
    return (eax >> 8) & 0x0F;
}

/**
 * @brief Get the CPU brand string
 */
char *cpu_getBrandString() {
    char brand[48];

    snprintf(brand, 10, "Unknown");

    uint32_t eax, unused;
    __cpuid(CPUID_INTELEXTENDED, eax, unused, unused, unused);
    if (eax >= CPUID_INTELBRANDSTRINGEND) {
        // Supported!
        uint32_t brand_data[12];
		__cpuid(0x80000002, brand_data[0], brand_data[1], brand_data[2], brand_data[3]);
		__cpuid(0x80000003, brand_data[4], brand_data[5], brand_data[6], brand_data[7]);
		__cpuid(0x80000004, brand_data[8], brand_data[9], brand_data[10], brand_data[11]);
        memcpy(brand, brand_data, 48);
    }

    // !!!: this is ugly
    return strdup(brand);
}