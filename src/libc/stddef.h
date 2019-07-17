#pragma once

#include "types.h"
#include "inttypes.h"

typedef uint64 ptrdiff_t;
typedef uint64 size_t;
typedef decltype(nullptr) nullptr_t;

#ifndef __cplusplus
typedef int16 wchar_t;
#endif

#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL (void*)0
#endif

#define offsetof(type, member) (&((type*)NULL)->member)