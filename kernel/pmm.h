#ifndef PMM_H
#define PMM_H

#include "multiboot.h"

#define PAGE_SIZE 4096

// PMM'yi başlatır
void init_pmm(multiboot_info_t* mbd);

// Bir adet fiziksel sayfa (page) tahsis eder
void* pmm_alloc_page();

// Bir sayfayı serbest bırakır (bump allocator için şimdilik boş)
void pmm_free_page(void* p);

// Shell ile uyumluluk için kullanılacak sayaç fonksiyonları
u32 pmm_get_used_mem();
u32 pmm_get_total_mem();

#endif