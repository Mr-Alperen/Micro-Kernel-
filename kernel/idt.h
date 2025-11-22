#ifndef IDT_H
#define IDT_H

#include "utils.h" // u16, u32 gibi tanımlamalar için (yeni dosya)

// IDT'deki bir girdinin (gate) yapısı
struct idt_entry {
    u16 base_lo;    // Handler fonksiyon adresinin alt 16 biti
    u16 sel;        // Kernel kod segmenti
    u8  always0;    // Her zaman sıfır olmalı
    u8  flags;      // Gate tipi ve özellikleri
    u16 base_hi;    // Handler fonksiyon adresinin üst 16 biti
} __attribute__((packed));

// IDT'yi CPU'ya yüklemek için kullanılacak pointer yapısı
struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));


// Fonksiyon prototipleri
void init_idt();

// ISR'ler (Assembly'de tanımlanacaklar)
extern void isr0();
extern void isr1();
// ... (tüm ISR'ler için bildirimler eklenebilir)
extern void isr33(); // Klavye için

#endif