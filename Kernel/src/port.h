#pragma once

#include "types.h"

namespace Port
{
    void OutByte(uint16 port, uint8 val);
    uint8 InByte(uint16 port);
}