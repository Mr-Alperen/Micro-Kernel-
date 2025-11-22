#include "shell.h"
#include "vga.h"
#include <stddef.h>
#include "string.h" // Yeni
#include "pmm.h"    // Yeni
#include "utils.h"

#define PROMPT "MK++ > "
#define MAX_CMD_LEN 256

// Komut tamponu ve mevcut pozisyon
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_pos = 0;

// --- Komut Fonksiyonları ---
// Her komut aynı imzaya sahip olmalı: int func(int argc, char** argv)

int cmd_help(int argc, char** argv);
int cmd_echo(int argc, char** argv);
int cmd_memstat(int argc, char** argv);
int cmd_panic_test(int argc, char** argv);
int cmd_clear(int argc, char** argv);

// --- Komut Tablosu ---
// Yeni bir komut eklemek için buraya bir satır eklemek yeterlidir.

typedef int (*shell_func_t)(int argc, char** argv);

typedef struct {
    const char* name;
    const char* description;
    shell_func_t func;
} shell_command_t;

const shell_command_t commands[] = {
    {"help", "Displays this help message.", cmd_help},
    {"echo", "Prints back its arguments.", cmd_echo},
    {"memstat", "Displays physical memory usage.", cmd_memstat},
    {"clear", "Clears the screen.", cmd_clear},
    {"panic", "Tests the kernel panic.", cmd_panic_test},
    {0, 0, 0} // Tablonun sonunu işaretler
};

// --- Komut İşleme Mantığı ---

static void shell_execute_command(char* input) {
    char* argv[32]; // Maksimum 32 argüman
    int argc = 0;

    char* token = strtok(input, " ");
    while (token != NULL && argc < 31) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    argv[argc] = NULL;

    if (argc == 0) {
        return; // Boş komut
    }

    // Komutu tabloda ara ve çalıştır
    for (int i = 0; commands[i].name != 0; i++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }
    
    write_vga_at("Command not found: ", -1, -1, 0x0C);
    write_vga_at(argv[0], -1, -1, 0x0C);
    write_char_at('\n', -1, -1, 0x07);
}

// --- Komut Fonksiyonlarının Implementasyonu ---

int cmd_help(int argc, char** argv) {
    write_vga_at("MicroKernel++ Shell - v0.4\nAvailable commands:\n", -1, -1, 0x0A);
    for (int i = 0; commands[i].name != 0; i++) {
        write_vga_at("  ", -1, -1, 0x07);
        write_vga_at(commands[i].name, -1, -1, 0x0E);
        write_vga_at("\t- ", -1, -1, 0x07);
        write_vga_at(commands[i].description, -1, -1, 0x07);
        write_char_at('\n', -1, -1, 0x07);
    }
    return 0;
}

int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        write_vga_at(argv[i], -1, -1, 0x0F);
        write_char_at(' ', -1, -1, 0x0F);
    }
    write_char_at('\n', -1, -1, 0x07);
    return 0;
}

int cmd_memstat(int argc, char** argv) {
    char buffer[12];
    u32 used_mem = pmm_get_used_mem();
    u32 total_mem = pmm_get_total_mem();
    
    write_vga_at("Physical Memory Usage:\n", -1, -1, 0x0B);
    
    utoa(used_mem / 1024, buffer, 10);
    write_vga_at("  Used: ", -1, -1, 0x07);
    write_vga_at(buffer, -1, -1, 0x0F);
    write_vga_at(" KB\n", -1, -1, 0x07);

    utoa(total_mem / 1024, buffer, 10);
    write_vga_at("  Total: ", -1, -1, 0x07);
    write_vga_at(buffer, -1, -1, 0x0F);
    write_vga_at(" KB\n", -1, -1, 0x07);

    return 0;
}

int cmd_panic_test(int argc, char** argv) {
    panic("User-initiated panic test.");
    return 0; // Buraya asla ulaşılmaz
}

int cmd_clear(int argc, char** argv) {
    clear_screen();
    return 0;
}

// --- Ana Shell Döngüsü ve Girdi İşleme ---

void shell_main_loop() {
    write_vga_at(PROMPT, -1, -1, 0x0A);
}

void shell_handle_keypress(char c) {
    if (c == '\n') { // Enter
        write_char_at(c, -1, -1, 0x07);
        if (cmd_pos > 0) {
            cmd_buffer[cmd_pos] = '\0'; // String'i sonlandır
            shell_execute_command(cmd_buffer);
        }
        cmd_pos = 0;
        write_vga_at(PROMPT, -1, -1, 0x0A);
    } else if (c == '\b') { // Backspace
        if (cmd_pos > 0) {
            cmd_pos--;
            write_char_at('\b', -1, -1, 0x07);
        }
    } else if (cmd_pos < MAX_CMD_LEN - 1) {
        cmd_buffer[cmd_pos++] = c;
        write_char_at(c, -1, -1, 0x0F);
    }
}