#pragma once

#define isalnum(x) (((x) >= '0' && (x) <= '9') || ((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z'))

#define isalpha(x) (((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z'))

#define isblank(x) ((x) == '\t' || (x) == ' ')

#define iscntrl(x) ((x) <= 0x1F || (x) == 0x7F)

#define isdigit(x) ((x) >= '0' && (x) <= '9')

#define isgraph(x) ((x) >= 0x21 && (x) <= 0x7E)

#define islower(x) ((x) >= 'a' && (x) <= 'z')

#define isprint(x) ((x) >= 0x20 && (x) <= 0x7E)

#define ispunct(x) (((x) >= 0x21 && (x) <= 0x2F) || ((x) >= 0x3A && (x) <= 0x40) || ((x) >= 0x5B && (x) <= 0x60) || ((x) >= 0x7B && (x) <= 0x7E))

#define isspace(x) (((x) >= 0x09 && (x) <= 0x0D) || (x) == ' ')

#define isupper(x) ((x) >= 'A' && (x) <= 'Z')

#define isxdigit(x) (isdigit(x) || ((x) >= 'a' && (x) <= 'f') || ((x) >= 'A' && (x) <= 'F'))

#define tolower(x) (isupper(x) ? ((x) | 0x20) : (x))

#define toupper(x) (islower(x) ? ((x) ^ 0x20) : (x))