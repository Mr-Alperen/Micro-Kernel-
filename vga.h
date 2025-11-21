
#ifndef VGA_H
#define VGA_H

#include "utils.h"

void write_vga_at(const char* s, int row, int col, u8 attr);
void write_char_at(char c, int row, int col, u8 attr);
void clear_screen();

#endif