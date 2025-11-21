#include "string.h"
#include "utils.h"

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Basit, re-entrant olmayan strtok
char* strtok(char* str, const char* delim) {
    static char* last;
    if (str) {
        last = str;
    }
    if (!last || *last == '\0') {
        return NULL;
    }

    char* token_start = last;
    while (*last != '\0') {
        const char* d = delim;
        while (*d != '\0') {
            if (*last == *d) {
                *last = '\0';
                last++;
                return token_start;
            }
            d++;
        }
        last++;
    }
    return token_start;
}

char* utoa(unsigned int value, char* str, int base) {

    if (base < 2 || base > 36) { *str = '\0'; return str; }
    char* ptr = str, *ptr1 = str, tmp_char;
    unsigned int tmp_value;
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
    } while (value);
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return str;
}