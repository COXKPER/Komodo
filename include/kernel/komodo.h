/*
 *
 *      komodo.h
 *      Kernel description header file
 *
 *      2024/7/23 By Rainy101112
 *      Copyright (C) 2020 ViudiraTech, based on the Apache 2.0 license.
 *
 */

#ifndef INCLUDE_KOMODO_H_
#define INCLUDE_KOMODO_H_

#define BUILD_DATE     __DATE__
#define BUILD_TIME     __TIME__
#define KERNEL_NAME    "Komodo"
/*
 * Version scheme: (total-commit).(changes).(updates)
 *   total-commit — `git rev-list --count HEAD` at release time
 *   changes      — number of feature/major changes since last update bump
 *   updates      — number of maintenance releases
 * See versioning.md at the EntoriNext root for the full policy.
 */
#define KERNEL_VERSION "630.0.0"

/* Compiler judgment */
#if defined(__clang__)
#    define COMPILER_NAME    "clang"
#    define STRINGIFY(x)     #x
#    define EXPAND(x)        STRINGIFY(x)
#    define COMPILER_VERSION EXPAND(__clang_major__.__clang_minor__.__clang_patchlevel__)
#elif defined(__GNUC__)
#    define COMPILER_NAME    "gcc"
#    define STRINGIFY(x)     #x
#    define EXPAND(x)        STRINGIFY(x)
#    define COMPILER_VERSION EXPAND(__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__)
#else
#    error "Unknown compiler"
#endif

#define KERNEL_BASE_ADDRESS 0xffffffff80000000

extern volatile struct limine_rsdp_request           rsdp_request;
extern volatile struct limine_kernel_file_request    kernel_file_request;
extern volatile struct limine_smp_request            smp_request;
extern volatile struct limine_framebuffer_request    framebuffer_request;
extern volatile struct limine_smbios_request         smbios_request;
extern volatile struct limine_memmap_request         memmap_request;
extern volatile struct limine_hhdm_request           hhdm_request;
extern volatile struct limine_kernel_address_request kernel_address_request;
extern volatile struct limine_entry_point_request    entry_point_request;
extern volatile struct limine_module_request         module_request;

/* Executable entry */
void executable_entry(void);

/* Kernel entry */
void kernel_entry(void);

#endif // INCLUDE_KOMODO_H_
