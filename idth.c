#include "idt.h"
#include "io.h"     // Port I/O için (yeni dosya)
#include "vga.h"    // VGA yazma fonksiyonları için (kernel.c'den taşınacak)

#define IDT_ENTRIES 256

struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr   idtp;

// IDT'ye bir gate (kapı) ekleyen yardımcı fonksiyon
static void set_idt_gate(u8 num, u32 base, u16 sel, u8 flags) {
    idt[num].base_lo = (base & 0xFFFF);
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

// PIC'i yeniden programla
static void init_pic(void) {
    // Master PIC
    outb(0x20, 0x11);
    outb(0x21, 0x20); // Master PIC interrupt'ları 32-39 arasına map et
    outb(0x21, 0x04);
    outb(0x21, 0x01);

    // Slave PIC
    outb(0xA0, 0x11);
    outb(0xA1, 0x28); // Slave PIC interrupt'ları 40-47 arasına map et
    outb(0xA1, 0x02);
    outb(0xA1, 0x01);

    // Maskeleme (sadece klavye interrupt'una izin ver)
    outb(0x21, 0xFD); // Sadece IRQ1 (klavye) açık
    outb(0xA1, 0xFF);
}

// IDT'yi kur ve yükle
void init_idt() {
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base  = (u32)&idt;

    // Tüm IDT girdilerini sıfırla
    for (int i = 0; i < IDT_ENTRIES; i++) {
        set_idt_gate(i, 0, 0, 0);
    }
    
    init_pic();

    // Klavye kesmesi için handler'ı kur (IRQ 1 -> Interrupt 33)
    set_idt_gate(33, (u32)isr33, 0x08, 0x8E);

    // IDT'yi yükle
    asm volatile ("lidt %0" : : "m"(idtp));
    // Kesmeleri aktive et
    asm volatile ("sti");
}

// C tabanlı genel kesme handler'ı
void isr_handler(u32 int_num) {
    if (int_num == 33) { // Klavye
        // Klavye handler'ını çağır
        keyboard_handler();
    }
    
    // İşlem bittiğinde PIC'e sinyal gönder (End of Interrupt)
    if (int_num >= 40) {
        outb(0xA0, 0x20); // Slave'e gönder
    }
    outb(0x20, 0x20); // Master'a gönder
}