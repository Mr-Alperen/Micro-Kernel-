#ifndef UTILS_H
#define UTILS_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

// Simple panic wrapper used across kernel modules
void panic(const char* msg);

#endif