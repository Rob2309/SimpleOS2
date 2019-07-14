#pragma once

#define CHAR_BIT 8

#define SCHAR_MIN (char)0x80
#define SCHAR_MAX (char)0x7F

#define UCHAR_MAX (unsigned char)0xFF;

#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX

#define MB_LEN_MAX 1

#define SHRT_MIN (short)0x8000
#define SHRT_MAX (short)0x7FFF

#define USHRT_MAX (unsigned short)0xFFFF

#define INT_MIN (int)0x80000000
#define INT_MAX (int)0x7FFFFFFF

#define UINT_MAX (unsigned int)0xFFFFFFFF

#define LONG_MIN (long)0x80000000
#define LONG_MAX (long)0x7FFFFFFF

#define ULONG_MAX (unsigned long)0xFFFFFFFF

#define LLONG_MIN (long long)0x8000000000000000
#define LLONG_MAX (long long)0x7FFFFFFFFFFFFFFF

#define ULLONG_MAX (unsigned long long)0xFFFFFFFFFFFFFFFF