#pragma once

#include "types.h"

namespace Terminal {
    void Init(volatile uint32* videoBuffer, int width, int height, int scanline, bool invertColors);

    /**
     * Move the Cursor to the given Coordinates
     **/
    void SetCursor(int x, int y);
    /**
     * Put a Single Character onto the Screen at the Cursor Position.
     * Automatically advances the Cursor.
     **/
    void PutChar(char c, uint32 color = 0xFFFFFFFF);
    /**
     * Move the cursor to the beginning of the next line
     **/
    void NewLine();

    /**
     * Clear the screen
     **/
    void Clear();
}