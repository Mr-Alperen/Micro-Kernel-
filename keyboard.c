#include "keyboard.h"
#include "io.h"
#include "vga.h"

// Basit US QWERTY klavye haritası
const char scancode_to_ascii[] = {
    '?', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    '?', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '?',
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '?', '?', '?', ' '
};

static int cursor_x = 0;
static int cursor_y = 3; // Başlangıç satırı

void keyboard_handler() {
    u8 scancode = inb(0x60); // Klavye portundan scancode'u oku

    if (scancode < sizeof(scancode_to_ascii)) {
        char c = scancode_to_ascii[scancode];
        if (c == '\n') {
            cursor_y++;
            cursor_x = 0;
        } else if (c == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
                write_char_at(' ', cursor_y, cursor_x, 0x07);
            }
        } else {
            write_char_at(c, cursor_y, cursor_x++, 0x0F);
            if (cursor_x >= 80) {
                cursor_x = 0;
                cursor_y++;
            }
        }
    }
}