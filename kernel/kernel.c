#include "vga.h"
#include "idt.h"
#include "multiboot.h" // Yeni
#include "pmm.h"       // Yeni

// Basit bir integer'ı hex string'e çeviren yardımcı fonksiyon
void hex_to_str(u32 n, char* out) {
    const char* hex = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        out[i + 2] = hex[(n >> (i * 4)) & 0xF];
    }
    out[10] = '\0';
}

// kernel_main artık multiboot info pointer'ını argüman olarak alıyor.
void kernel_main(multiboot_info_t* mbd) {
    char buffer[11];
    clear_screen();
    write_vga_at("MicroKernel++ v0.3", 0, 0, 0x07);

    write_vga_at("Initializing Interrupts...", 1, 0, 0x07);
    init_idt();
    write_vga_at("OK", 1, 27, 0x02);
    
    write_vga_at("Initializing Physical Memory Manager...", 2, 0, 0x07);
    init_pmm(mbd);
    write_vga_at("OK", 2, 38, 0x02);

    write_vga_at("Keyboard enabled. Type something:", 4, 0, 0x0F);
    
    // Bellek yöneticisini test edelim
    write_vga_at("Testing PMM: Allocating 3 pages...", 8, 0, 0x0B);
    
    void* p1 = pmm_alloc_page();
    hex_to_str((u32)p1, buffer);
    write_vga_at("Page 1 allocated at: ", 9, 2, 0x0A);
    write_vga_at(buffer, 9, 25, 0x0E);
    
    void* p2 = pmm_alloc_page();
    hex_to_str((u32)p2, buffer);
    write_vga_at("Page 2 allocated at: ", 10, 2, 0x0A);
    write_vga_at(buffer, 10, 25, 0x0E);

    void* p3 = pmm_alloc_page();
    hex_to_str((u32)p3, buffer);
    write_vga_at("Page 3 allocated at: ", 11, 2, 0x0A);
    write_vga_at(buffer, 11, 25, 0x0E);

    for (;;) {
        asm volatile ("hlt");
    }
}
