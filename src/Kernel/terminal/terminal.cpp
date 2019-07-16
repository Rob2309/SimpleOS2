#include "terminal.h"

#include "terminalfont.h"
#include "klib/memory.h"

namespace Terminal {
    constexpr int g_Margin = 25;

    void InitTerminalInfo(TerminalInfo* tInfo, volatile uint32* videoBuffer, int width, int height, int scanline, bool invertColors)
    {
        tInfo->cursorX = 0;
        tInfo->cursorY = 0;

        tInfo->screenCharsPerRow = (width - g_Margin * 2) / Font::g_CharWidth;
        tInfo->screenCharsPerCol = (height - g_Margin * 2) / Font::g_CharHeight;

        tInfo->screenWidth = width;
        tInfo->screenHeight = height;
        tInfo->screenScanlineWidth = scanline;

        tInfo->vBuffer = (uint32*)videoBuffer;

        tInfo->invertColors = invertColors;
    }

    static void Blt(uint32* dest, uint32* src, int srcX, int srcY, int dstX, int dstY, int width, int height, int srcScan, int dstScan)
    {
        for(int y = 0; y < height; y++) {
            kmemcpy(&dest[(dstY + y) * dstScan], &src[(srcY + y) * srcScan], width);
        }
    }

    static void Fill(uint32* dest, uint32 pxl, int x, int y, int w, int h, int dstScan)
    {
        for(int yy = y; yy < y + h; yy++) {
            for(int xx = x; xx < x + w; xx++) {
                dest[xx + yy * dstScan] = pxl;
            }
        }
    }

    static void RenderChar(TerminalInfo* tInfo, char c, int xPos, int yPos, uint32 color) {
		uint8* charData = &Font::g_FontData[(uint64)c * Font::g_CharHeight];

		for(int y = 0; y < Font::g_CharHeight; y++) {
			for(int x = 0; x < Font::g_CharWidth; x++) {
				if(charData[y] & (1 << (Font::g_CharWidth - 1 - x)))
					tInfo->vBuffer[(xPos * Font::g_CharWidth + g_Margin + x) + (yPos * Font::g_CharHeight + g_Margin + y) * tInfo->screenScanlineWidth] = color;
				else
					tInfo->vBuffer[(xPos * Font::g_CharWidth + g_Margin + x) + (yPos * Font::g_CharHeight + g_Margin + y) * tInfo->screenScanlineWidth] = 0;
			}
		}
	}

    static void AdvanceCursor(TerminalInfo* tInfo)
    {
        tInfo->cursorX++;
        if(tInfo->cursorX == tInfo->screenCharsPerRow) {
            tInfo->cursorX = 0;
            tInfo->cursorY++;

            if(tInfo->cursorY == tInfo->screenCharsPerCol) {
                tInfo->cursorY = tInfo->screenCharsPerCol - 1;

                Blt(tInfo->vBuffer, tInfo->vBuffer, g_Margin, g_Margin + Font::g_CharHeight, g_Margin, g_Margin, tInfo->screenCharsPerRow * Font::g_CharWidth, (tInfo->screenCharsPerCol - 1) * Font::g_CharHeight, tInfo->screenScanlineWidth, tInfo->screenScanlineWidth);
                Fill(tInfo->vBuffer, 0, g_Margin, g_Margin + (tInfo->screenCharsPerCol - 1) * Font::g_CharHeight, tInfo->screenCharsPerRow * Font::g_CharWidth, Font::g_CharHeight, tInfo->screenScanlineWidth);
            }
        }
    }

    void SetCursor(TerminalInfo* tInfo, int x, int y)
    {
        tInfo->cursorX = x;
        tInfo->cursorY = y;
    }
    void PutChar(TerminalInfo* tInfo, char c, uint32 color)
    {
        if(tInfo->invertColors) {
            uint8 r = (color >> 16) & 0xFF;
            uint8 g = (color >> 8) & 0xFF;
            uint8 b = (color) & 0xFF;
            color = (b << 16) | (g << 8) | r;
        }

        RenderChar(tInfo, c, tInfo->cursorX, tInfo->cursorY, color);
        AdvanceCursor(tInfo);
    }
    void NewLine(TerminalInfo* tInfo)
    {
        tInfo->cursorX = tInfo->screenCharsPerRow - 1;
        AdvanceCursor(tInfo);
    }

    void Clear(TerminalInfo* tInfo)
    {
        Fill(tInfo->vBuffer, 0, g_Margin, g_Margin, tInfo->screenCharsPerRow * Font::g_CharWidth, tInfo->screenCharsPerCol * Font::g_CharHeight, tInfo->screenScanlineWidth);
    }
}
