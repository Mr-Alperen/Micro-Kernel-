#include <stdint.h>
#include "pmm.h"
#include "utils.h"
#include "vga.h" // Hata mesajları için

// Linker script'ten gelen `end` sembolü
extern u32 end;

// Bellek yöneticimizin durumu
static u32 pmm_memory_end = 0;
static u32 pmm_current_break = 0;
static u32 pmm_start_addr = 0;

void init_pmm(multiboot_info_t* mbd) {
    // Multiboot yapısında mmap bayrağı set edilmiş mi kontrol et
    if (!(mbd->flags & MBOOT_FLAG_MMAP)) {
        panic("Memory map not provided by bootloader!");
        return;
    }

    // Kullanılabilir en büyük RAM bloğunu bul
    u64 best_region_base = 0;
    u64 best_region_len = 0;

    memory_map_t* mmap = (memory_map_t*)mbd->mmap_addr;
    while ((u32)mmap < mbd->mmap_addr + mbd->mmap_length) {
        if (mmap->type == 1) { // 1 = Kullanılabilir RAM
            if (mmap->length > best_region_len) {
                best_region_base = mmap->base_addr;
                best_region_len = mmap->length;
            }
        }
        mmap = (memory_map_t*)((u32)mmap + mmap->size + sizeof(u32));
    }
    
    // Bump allocator'ın başlangıç noktasını ayarla.
    // Başlangıç, çekirdeğin sonundan sonrası olmalı.
    u32 kernel_end_addr = (u32)&end;
    pmm_current_break = kernel_end_addr;

    // Başlangıç adresini sayfa sınırına yuvarla (align up)
    if (pmm_current_break % PAGE_SIZE != 0) {
        pmm_current_break = (pmm_current_break / PAGE_SIZE + 1) * PAGE_SIZE;
    }

    // Belleğin sonunu belirle
    pmm_memory_end = (u32)best_region_base + (u32)best_region_len;

    if (pmm_current_break >= pmm_memory_end) {
        panic("Not enough memory to start PMM!");
    }

    // Kaydet: başlangıç adresi (kullanılan bellek hesabı için)
    pmm_start_addr = pmm_current_break;
}

// Bump allocator mantığı
void* pmm_alloc_page() {
    if (pmm_current_break + PAGE_SIZE > pmm_memory_end) {
        // Bellek tükendi!
        return 0; 
    }

    void* allocated_page = (void*)pmm_current_break;
    pmm_current_break += PAGE_SIZE;
    return allocated_page;
}

// Basit bir bump allocator'da 'free' işlemi yoktur.
// Şimdilik bu fonksiyon hiçbir şey yapmaz.
void pmm_free_page(void* p) {
    // No-op
}

// Basit wrapper'lar: shell'in beklediği isimlerle uyum sağlamak için
u32 pmm_get_used_mem() {
    if (pmm_current_break < pmm_start_addr) return 0;
    return pmm_current_break - pmm_start_addr;
}

u32 pmm_get_total_mem() {
    if (pmm_memory_end <= pmm_start_addr) return 0;
    return pmm_memory_end - pmm_start_addr;
}

/* Compatibility wrappers for older/other naming in coresystem.c */
uint32_t pmm_get_used_memory() {
    return pmm_get_used_mem();
}

uint32_t pmm_get_free_memory() {
    return pmm_get_total_mem() - pmm_get_used_mem();
}