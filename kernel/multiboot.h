#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "utils.h"

#define MBOOT_FLAG_MEM     0x001
#define MBOOT_FLAG_DEVICE  0x002
#define MBOOT_FLAG_CMDLINE 0x004
#define MBOOT_FLAG_MODS    0x008
#define MBOOT_FLAG_AOUT    0x010
#define MBOOT_FLAG_ELF     0x020
#define MBOOT_FLAG_MMAP    0x040
#define MBOOT_FLAG_DRIVE   0x080
#define MBOOT_FLAG_CONFIG  0x100
#define MBOOT_FLAG_LOADER  0x200
#define MBOOT_FLAG_APM     0x400
#define MBOOT_FLAG_VBE     0x800

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[3];
    u32 mmap_length;
    u32 mmap_addr;
    // ... diğer alanlar
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    u32 size;
    u64 base_addr;
    u64 length;
    u32 type;
} __attribute__((packed)) memory_map_t;

#endif