#include "terminal.h"

#include "conio.h"
#include "terminalfont.h"

namespace Terminal {
    constexpr int g_Margin = 25;

    static uint32* g_VideoBuffer;

    static int g_CursorX, g_CursorY;

    static int g_ScreenCharsPerRow;
    static int g_ScreenCharsPerCol;

    static int g_ScreenWidth;
    static int g_ScreenScanline;
    static int g_ScreenHeight;

    static bool g_InvertColors;

    void Init(uint32* videoBuffer, int width, int height, int scanline, bool invertColors)
    {
        g_CursorX = 0;
        g_CursorY = 0;

        g_ScreenCharsPerRow = (width - g_Margin * 2) / Font::g_CharWidth;
        g_ScreenCharsPerCol = (height - g_Margin * 2) / Font::g_CharHeight;

        g_ScreenWidth = width;
        g_ScreenHeight = height;
        g_ScreenScanline = scanline;

        g_VideoBuffer = videoBuffer;

        g_InvertColors = invertColors;
    }

    static void Blt(uint32* dest, uint32* src, int srcX, int srcY, int dstX, int dstY, int width, int height, int srcScan, int dstScan)
    {
        for(int y = 0; y < height; y++)
            for(int x = 0; x < width; x++) {
                dest[(dstX + x) + (dstY + y) * dstScan] = src[(srcX + x) + (srcY + y) * srcScan];
            }
    }

    static void BltColor(uint32* dest, uint32* src, int srcX, int srcY, int dstX, int dstY, int width, int height, int srcScan, int dstScan, uint32 color)
    {
        for(int y = 0; y < height; y++)
            for(int x = 0; x < width; x++) {
                dest[(dstX + x) + (dstY + y) * dstScan] = src[(srcX + x) + (srcY + y) * srcScan] & color;
            }
    }

    static void Fill(uint32* dest, uint32 pxl, int x, int y, int w, int h, int dstScan)
    {
        for(int yy = y; yy < y + w; yy++) {
            for(int xx = x; xx < x + w; xx++) {
                dest[xx + yy * dstScan] = pxl;
            }
        }
    }

    static void RenderChar(char c, int xPos, int yPos, uint32 color) {
		uint8* charData = &Font::g_FontData[(uint64)c * Font::g_CharHeight];

		for(int y = 0; y < Font::g_CharHeight; y++) {
			for(int x = 0; x < Font::g_CharWidth; x++) {
				if(charData[y] & (1 << (Font::g_CharWidth - 1 - x)))
					g_VideoBuffer[(xPos * Font::g_CharWidth + g_Margin + x) + (yPos * Font::g_CharHeight + g_Margin + y) * g_ScreenScanline] = color;
				else
					g_VideoBuffer[(xPos * Font::g_CharWidth + g_Margin + x) + (yPos * Font::g_CharHeight + g_Margin + y) * g_ScreenScanline] = 0;
			}
		}
	}

    static void AdvanceCursor()
    {
        g_CursorX++;
        if(g_CursorX == g_ScreenCharsPerRow) {
            g_CursorX = 0;
            g_CursorY++;

            if(g_CursorY == g_ScreenCharsPerCol) {
                g_CursorY = g_ScreenCharsPerCol - 1;

                Blt(g_VideoBuffer, g_VideoBuffer, g_Margin, g_Margin + Font::g_CharHeight, g_Margin, g_Margin, g_ScreenCharsPerRow * Font::g_CharWidth, (g_ScreenCharsPerCol - 1) * Font::g_CharHeight, g_ScreenScanline, g_ScreenScanline);
                Fill(g_VideoBuffer, 0, g_Margin, g_Margin + (g_ScreenCharsPerCol - 1) * Font::g_CharHeight, g_ScreenCharsPerRow * Font::g_CharWidth, Font::g_CharHeight, g_ScreenScanline);
            }
        }
    }

    void SetCursor(int x, int y)
    {
        g_CursorX = x;
        g_CursorY = y;
    }
    void PutChar(char c, uint32 color)
    {
        if(g_InvertColors) {
            uint8 r = (color >> 16) & 0xFF;
            uint8 g = (color >> 8) & 0xFF;
            uint8 b = (color) & 0xFF;
            color = (b << 16) | (g << 8) | r;
        }

        RenderChar(c, g_CursorX, g_CursorY, color);
        AdvanceCursor();
    }
    void NewLine()
    {
        g_CursorX = g_ScreenCharsPerRow - 1;
        AdvanceCursor();
    }

    void Clear()
    {
        Fill(g_VideoBuffer, 0, g_Margin, g_Margin, g_ScreenCharsPerRow * Font::g_CharWidth, g_ScreenCharsPerCol * Font::g_CharHeight, g_ScreenScanline);
    }
}
