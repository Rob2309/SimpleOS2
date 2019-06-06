#pragma once

int kstrcmp(const char* a, const char* b);

int kstrcmp(const char* a, int aStart, int aEnd, const char* b);

int kstrlen(const char* a);

void kstrcpy(char* dest, const char* src);

void kstrconcat(char* dest, const char* a, const char* b);