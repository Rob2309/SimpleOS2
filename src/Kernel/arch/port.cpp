#include "port.h"

namespace Port
{
    void OutByte(uint16 port, uint8 val)
    {
        __asm__ __volatile__ (
            "outb %%al, %%dx"
            : : "a"(val), "d"(port)
        );
    }
    uint8 InByte(uint16 port)
    {
        uint8 ret = 0;
        __asm__ __volatile__ (
            "inb %%dx, %%al"
            : "=a" (ret)
            : "d" (port)
        );
        return ret;
    }

    void OutDWord(uint16 port, uint32 val) {
        __asm__ __volatile__ (
            "outl %%eax, %%dx"
            : : "a"(val), "d"(port)
        );
    }
    uint32 InDWord(uint16 port) {
        uint32 ret = 0;
        __asm__ __volatile__ (
            "inl %%dx, %%eax"
            : "=a" (ret)
            : "d" (port)
        );
        return ret;
    }
}