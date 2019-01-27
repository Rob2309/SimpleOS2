#include "console.h"

#include "efiutil.h"
#include "file.h"
#include "memory.h"

static EFI_GRAPHICS_OUTPUT_BLT_PIXEL* g_FontBuffer;
static int g_CursorX, g_CursorY;

static int g_CharWidth = 9;
static int g_CharHeight = 16;

static int g_FontCharsPerRow = 16;

static int g_CharsPerRow;
static int g_CharsPerCol;

static int g_ScreenWidth;
static int g_ScreenHeight;

void ConsoleInit()
{
    FILE* fontFile = fopen(L"EFI\\BOOT\\font.bin");
    uint64 fontSize = fgetsize(fontFile);
    g_FontBuffer = malloc(fontSize);
    fread(g_FontBuffer, fontSize, fontFile);
    fclose(fontFile);

    g_EFISystemTable->ConOut->ClearScreen(g_EFISystemTable->ConOut);

    g_CursorX = 0;
    g_CursorY = 0;

    UINTN infoSize;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
    g_EFIGraphics->QueryMode(g_EFIGraphics, g_EFIGraphics->Mode->Mode, &infoSize, &info);

    g_CharsPerRow = info->HorizontalResolution / g_CharWidth;
    g_CharsPerCol = info->VerticalResolution / g_CharHeight;

    g_ScreenWidth = info->HorizontalResolution;
    g_ScreenHeight = info->VerticalResolution;
}
void ConsoleCleanup()
{
    free(g_FontBuffer);
}

static void RenderChar(char c, int x, int y) {
    int srcX = (c % g_FontCharsPerRow) * g_CharWidth;
    int srcY = (c / g_FontCharsPerRow) * g_CharHeight;
    g_EFIGraphics->Blt(g_EFIGraphics, g_FontBuffer, EfiBltBufferToVideo, srcX, srcY, x * g_CharWidth, y * g_CharHeight, g_CharWidth, g_CharHeight, g_FontCharsPerRow * g_CharWidth * 4);
}

static void AdvanceCursor() 
{
    g_CursorX++;
    if(g_CursorX == g_CharsPerRow) {
        g_CursorX = 0;
        g_CursorY++;

        if(g_CursorY == g_CharsPerCol) {
            g_CursorY = g_CharsPerCol - 1;

            g_EFIGraphics->Blt(g_EFIGraphics, NULL, EfiBltVideoToVideo, 0, g_CharHeight, 0, 0, g_ScreenWidth, g_ScreenHeight - g_CharHeight, 0);

            EFI_GRAPHICS_OUTPUT_BLT_PIXEL blackPixel = { 0 };
            g_EFIGraphics->Blt(g_EFIGraphics, &blackPixel, EfiBltVideoFill, 0, 0, 0, g_CursorY * g_CharHeight, g_CharWidth * g_CharsPerRow, g_CharHeight, 0);
        }
    }
}

void SetCursor(int x, int y)
{
    g_CursorX = x;
    g_CursorY = y;
}
void PutChar(char c)
{
    RenderChar(c, g_CursorX, g_CursorY);
    AdvanceCursor();
}
void NewLine() {
    g_CursorX = g_CharsPerRow - 1;
    AdvanceCursor();
}