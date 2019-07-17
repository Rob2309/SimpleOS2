#pragma once

#ifndef NDEBUG
void __do_assert(int cond, const char* msg);
#   define _STRINGIFY(x) #x
#   define STRINGIFY(x) _STRINGIFY(x)
#   define assert(cond) __do_assert((cond), "Assertion failed: " STRINGIFY(cond) ", file " __FILE__ ", line " STRINGIFY(__LINE__) "\n")
#else
#   define assert(cond) 
#endif