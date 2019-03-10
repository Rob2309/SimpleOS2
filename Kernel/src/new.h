#pragma once

inline void* operator new(long unsigned int size, void* loc)
{
    return loc;
}
inline void* operator new[](long unsigned int size, void* loc)
{
    return loc;
}