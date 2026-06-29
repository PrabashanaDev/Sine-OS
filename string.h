#ifndef STRING_H
#define STRING_H

typedef unsigned int size_t;

size_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
void* memset(void* dest, int val, size_t len);
void itoa(int num, char* str, int base);
void reverse(char* str, int length);

#endif
