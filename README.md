# ====== Micro Kernel++ Project =====

#### The MicroKernel++ Project is still under development and is open to anyone wishing to contribute, but it is a project undertaken by Stux6 Tech. Contribute!

e-mail : stux6.team@gmail.com
instagram : stux6redteam


### MicroKernel++ — Initial bootstrap (v0.1)

This initial commit provides a Multiboot-compatible kernel stub written in C and a tiny multiboot header assembly file so you can boot with GRUB (recommended) or test in QEMU. The goal of this first step is to get a small kernel running in protected mode and print to the VGA text buffer.

**What is included**
- `boot/boot.S`  — Multiboot header + trivial assembly entry (i386, AT/PC)
- `kernel/kernel.c` — Minimal freestanding kernel in C: sets up VGA text printing, a basic panic, and a simple keyboard poll loop placeholder.
- `kernel/linker.ld` — Linker script to place sections correctly for a simple kernel binary.
- `Makefile` — build targets: `all`, `iso`, `run`, `clean`.
- `grub/` — grub.cfg to boot the kernel from an ISO.

**Prerequisites (on macOS)**
Install via Homebrew:
- `brew install qemu grub` (or `grub-mkrescue` by installing `grub`) 
- `x86_64-elf-gcc` toolchain recommended (or use system gcc but ensure -m32 support). On macOS you might prefer cross tools or build inside a VM/docker. The provided Makefile is written for a Linux environment — adapt paths if necessary.

**Build & Run (Linux / compatible environment)**

1. `make all`
2. `make iso`
3. `make run` (runs QEMU)

On macOS you can run the resulting `microkernel.iso` with QEMU: `qemu-system-i386 -cdrom microkernel.iso -m 512M -boot d`.

---

# === boot/boot.S ===

    .section .multiboot
    .align 4
    .globl _start

    /* Multiboot header */
    /* For simplicity we make a minimal multiboot header recognized by GRUB */
    .long 0x1BADB002      /* magic */
    .long 0x0             /* flags */
    .long -(0x1BADB002)   /* checksum */

    .section .text
    .align 4
_start:
    /* Entry point after GRUB loads the kernel; GRUB will set up protected mode. */
    /* Call kernel_main from C */
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang


# === kernel/kernel.c ===

/* Minimal freestanding kernel for MicroKernel++ (initial prototype) */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define VGA_ADDR ((volatile u16*)0xB8000)

static void write_vga_at(const char* s, int row, int col, u8 attr) {
    volatile u16* vga = VGA_ADDR + row*80 + col;
    while (*s) {
        *vga++ = ((u16)attr << 8) | (u8)(*s);
        s++;
    }
}

void kernel_main(void) {
    /* Very small init */
    write_vga_at("MicroKernel++ v0.1\n", 0, 0, 0x07);
    write_vga_at("Initializing...", 1, 0, 0x07);

    /* Placeholder: memory manager init */
    /* Placeholder: interrupt table install */

    write_vga_at("OK\n", 1, 14, 0x02);

    /* Simple main loop: show a running dot */
    int i = 0;
    char buf[64];
    while (1) {
        /* crude busy loop to show kernel alive */
        if ((i++ % 1000000) == 0) {
            write_vga_at(".", 2, (i/1000000) % 70, 0x0E);
        }
    }
}

/* Provide a minimal panic routine for later use */
void panic(const char* msg) {
    write_vga_at("PANIC: ", 24, 0, 0x4F);
    write_vga_at(msg, 24, 7, 0x4F);
    for (;;) asm volatile ("cli; hlt");
}


# === kernel/linker.ld ===

ENTRY(_start)

SECTIONS
{
  . = 0x00100000; /* load at 1MiB */

  .text : { *(.multiboot) *(.text) }
  .rodata : { *(.rodata) }
  .data : { *(.data) }
  .bss  : { *(.bss) }
}


# === grub/grub.cfg ===

set timeout=5
set default=0

menuentry "MicroKernel++" {
    multiboot /boot/kernel.bin
    boot
}


# === Makefile ===

# Simple Makefile — adapt toolchain as needed
CC := i386-elf-gcc
LD := i386-elf-ld
AS := i386-elf-as

OBJS := boot/boot.o kernel/kernel.o
KERN_BIN := build/kernel.bin
ISO := microkernel.iso

CFLAGS := -m32 -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-builtin
LDFLAGS := -m elf_i386 -T kernel/linker.ld

all: build/$(KERN_BIN)

boot/boot.o: boot/boot.S
	mkdir -p build
	$(AS) -o $@ $^

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c -o $@ $^

build/$(KERN_BIN): boot/boot.o kernel/kernel.o kernel/linker.ld
	$(LD) $(LDFLAGS) -o build/kernel.elf boot/boot.o kernel/kernel.o
	objcopy -O binary build/kernel.elf build/kernel.bin

iso: all
	mkdir -p iso/boot/grub
	cp build/kernel.bin iso/boot/
	cp grub/grub.cfg iso/boot/grub/
	grub-mkrescue -o $(ISO) iso || echo "If grub-mkrescue failed on macOS, build iso on Linux/VM"

run: iso
	qemu-system-i386 -cdrom $(ISO) -m 512M -boot d

clean:
	rm -rf build iso $(ISO) *.o boot/*.o kernel/*.o


# Notes and next steps (in-code roadmap)

/*
Next immediate features to implement (v0.2):
1) Install an IDT and minimal PIC remapping; write a keyboard interrupt handler to receive keystrokes.
2) Implement a simple physical memory map reader and small bump allocator.
3) Replace busy-loop with timer interrupts and task switching skeleton.
4) Add a tiny FAT12-based filesystem loader so kernel can load programs from a disk image.
5) Add a sandbox runner that can run a user program in ring 3 (if switching to protected mode tasks) or within a tightly instrumented emulator.
6) Start integrating a proof-spec: write small invariants in Coq (off-kernel) that describe the expected behavior of the allocator and show how to translate runtime checks into provable contracts.

For the formal verification piece, we'll scaffold a `specs/` directory and begin by specifying the allocator and simple syscall semantics in Coq. We will not attempt to extract kernel code from Coq yet — instead we will use Coq to prove invariants about pure algorithms and later try to connect with the kernel via verified boot hashes and runtime checks.
*/
