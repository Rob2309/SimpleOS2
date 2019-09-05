#pragma once

#include "types.h"

namespace Terminal {
    
    struct TerminalInfo {
        uint32* vBuffer;
        int screenWidth;
        int screenHeight;
        int screenScanlineWidth;
        bool invertColors;

        int cursorX, cursorY;
        int screenCharsPerRow;
        int screenCharsPerCol;

        uint32* bBuffer;
        int bBufferLine;
    };

    void InitTerminalInfo(TerminalInfo* tInfo, volatile uint32* videoBuffer, int width, int height, int scanline, bool invertColors);
    void EnableDoubleBuffering(TerminalInfo* tInfo);

    /**
     * Move the Cursor to the given Coordinates
     **/
    void SetCursor(TerminalInfo* tInfo, int x, int y);
    /**
     * Put a Single Character onto the Screen at the Cursor Position.
     * Automatically advances the Cursor.
     **/
    void PutChar(TerminalInfo* tInfo, char c, uint32 color = 0xFFFFFFFF);
    void RemoveChar(TerminalInfo* tInfo);
    /**
     * Move the cursor to the beginning of the next line
     **/
    void NewLine(TerminalInfo* tInfo);

    /**
     * Clear the screen
     **/
    void Clear(TerminalInfo* tInfo);
}