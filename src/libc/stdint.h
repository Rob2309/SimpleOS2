#pragma once

#include "types.h"
#include "limits.h"

typedef int8 int8_t;
typedef uint8 uint8_t;
typedef int16 int16_t;
typedef uint16 uint16_t;
typedef int32 int32_t;
typedef uint32 uint32_t;
typedef int64 int64_t;
typedef uint64 uint64_t;

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

typedef int8_t int_least8_t;
typedef uint8_t uint_least8_t;
typedef int16_t int_least16_t;
typedef uint16_t uint_least16_t;
typedef int32_t int_least32_t;
typedef uint32_t uint_least32_t;
typedef int64_t int_least64_t;
typedef uint64_t uint_least64_t;

typedef int8_t int_fast8_t;
typedef uint8_t uint_fast8_t;
typedef int16_t int_fast16_t;
typedef uint16_t uint_fast16_t;
typedef int32_t int_fast32_t;
typedef uint32_t uint_fast32_t;
typedef int64_t int_fast64_t;
typedef uint64_t uint_fast64_t;

typedef uint64 intptr_t;
typedef uint64 uintptr_t;

#define INTMAX_MIN LLONG_MIN
#define INTMAX_MAX LLONG_MAX
#define UINTMAX_MAX ULLONG_MAX