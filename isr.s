.section .text
.globl isr_handler_common
.extern isr_handler
.extern keyboard_handler

/* Hata kodu olmayan ISR'ler için makro */
%macro ISR_NOERR_STUB 1
.globl isr%1
isr%1:
    cli
    push %1
    jmp isr_handler_common
%endmacro

/* CPU istisnaları (ilk 32) ve klavye için stub'lar */
ISR_NOERR_STUB 0
ISR_NOERR_STUB 1
; ... (diğer istisnalar için de eklenebilir)
ISR_NOERR_STUB 33 /* Klavye */

/* Tüm ISR'lerin çağıracağı ortak C sarmalayıcısı */
isr_handler_common:
    pusha       /* Tüm genel amaçlı register'ları sakla */

    mov ax, ds  /* Kernel veri segmentini yükle */
    push ax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler /* C handler'ını çağır */

    pop ax      /* Orijinal veri segmentini geri yükle */
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa        /* Register'ları geri yükle */
    add esp, 8  /* Hata kodu ve interrupt numarasını stack'ten temizle */
    sti         /* Kesmeleri tekrar etkinleştir */
    iret        /* Kesmeden dön */