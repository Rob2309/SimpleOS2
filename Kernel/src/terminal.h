#pragma once

#include "types.h"

namespace Terminal {
    void Init(uint32* fontBuffer, uint32* videoBuffer, int width, int height, int scanline);

    void SetCursor(int x, int y);
    void PutChar(char c);
    void NewLine();

    void Clear();
}