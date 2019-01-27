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

static char* g_TextBuffer;

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

    g_TextBuffer = malloc(g_CharsPerRow * g_CharsPerCol);
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
static void RenderTextBuffer()
{
    for(int r = 0; r < g_CharsPerCol; r++) {
        for(int c = 0; c < g_CharsPerRow; c++) {
            RenderChar(g_TextBuffer[c + r * g_CharsPerRow], c, r);
        }
    }
}

static void AdvanceCursor() 
{
    g_CursorX++;
    if(g_CursorX == g_CharsPerRow) {
        g_CursorX = 0;
        g_CursorY++;

        if(g_CursorY == g_CharsPerCol) {
            g_CursorY = g_CharsPerCol - 1;

            for(int row = 0; row < g_CharsPerCol - 1; row++)
            {
                for(int col = 0; col < g_CharsPerRow; col++)
                    g_TextBuffer[col + row * g_CharsPerRow] = g_TextBuffer[col + (row + 1) * g_CharsPerRow];
            }
            for(int col = 0; col < g_CharsPerRow; col++)
                g_TextBuffer[col + (g_CharsPerCol - 1) * g_CharsPerRow] = '\0';

            RenderTextBuffer();
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
    g_TextBuffer[g_CursorX + g_CursorY * g_CharsPerRow] = c;
    RenderChar(c, g_CursorX, g_CursorY);

    AdvanceCursor();
}
void NewLine() {
    g_CursorX = g_CharsPerRow - 1;
    AdvanceCursor();
}