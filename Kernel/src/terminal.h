#pragma once

#include "types.h"

namespace Terminal {
    void Init(uint32* fontBuffer, uint32* videoBuffer, int width, int height, int scanline, bool invertColors);

    void SetCursor(int x, int y);
    void PutChar(char c, uint32 color = 0xFFFFFFFF);
    void NewLine();

    void Clear();
}