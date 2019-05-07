#pragma once

/**
 * Placement new operators require explicit definitions, in contrast to the normal new operators
 **/

inline void* operator new(long unsigned int size, void* loc)
{
    return loc;
}
inline void* operator new[](long unsigned int size, void* loc)
{
    return loc;
}