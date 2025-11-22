.section .multiboot
    .align 4
    .globl _start

    /* Multiboot header */
    .set MAGIC,    0x1BADB002      /* magic */
    /* Flags: Bit 0 (bellek bilgisi) ve Bit 6 (bellek haritası) set edilmeli */
    .set FLAGS,    (1<<0) | (1<<6)
    .set CHECKSUM, -(MAGIC + FLAGS)

    .long MAGIC
    .long FLAGS
    .long CHECKSUM

    .section .text
    .align 4
_start:
    /* Entry point after GRUB loads the kernel */
    
    /* Multiboot info struct adresi EBX'te. kernel_main'e argüman olarak ver. */
    push ebx
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang