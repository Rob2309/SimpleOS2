#include "console.h"

#include "efiutil.h"
#include "file.h"
#include "memory.h"

static EFI_GRAPHICS_OUTPUT_BLT_PIXEL* g_FontBuffer;
static int g_CursorX, g_CursorY;

static int g_CharWidth = 9;
static int g_CharHeight = 16;
static int g_FontCharsPerRow = 16;

static int g_LineWidth;

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
    g_LineWidth = info->HorizontalResolution / g_CharWidth;
}
void ConsoleCleanup()
{
    free(g_FontBuffer);
}

void SetCursor(int x, int y)
{
    g_CursorX = x;
    g_CursorY = y;
}
void PutChar(char c)
{
    int srcX = (c % g_FontCharsPerRow) * g_CharWidth;
    int srcY = (c / g_FontCharsPerRow) * g_CharHeight;
    g_EFIGraphics->Blt(g_EFIGraphics, g_FontBuffer, EfiBltBufferToVideo, srcX, srcY, g_CursorX * g_CharWidth, g_CursorY * g_CharHeight, g_CharWidth, g_CharHeight, g_FontCharsPerRow * g_CharWidth * 4);

    g_CursorX++;
    if(g_CursorX == g_LineWidth) {
        g_CursorX = 0;
        g_CursorY++;
    }
}
void NewLine() {
    g_CursorX = 0;
    g_CursorY++;
}