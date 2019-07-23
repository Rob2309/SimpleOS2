#pragma once

#include "types.h"

namespace Port
{
    /**
     * Writes a byte to the given port
     **/
    void OutByte(uint16 port, uint8 val);
    /**
     * Reads a byte from the given port
     **/
    uint8 InByte(uint16 port);

    void OutDWord(uint16 port, uint32 val);
    uint32 InDWord(uint16 port);
}