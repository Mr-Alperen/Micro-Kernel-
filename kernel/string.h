#ifndef STRING_H
#define STRING_H

int strcmp(const char* s1, const char* s2);
char* strtok(char* str, const char* delim);
char* utoa(unsigned int value, char* str, int base); // Unsigned int to string

#endif