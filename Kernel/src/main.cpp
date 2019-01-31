#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"

extern "C" void __attribute__((ms_abi)) __attribute__((noreturn)) main(KernelHeader* info) {
    
    uint32* fontBuffer = nullptr;
    for(int m = 0; m < info->numModules; m++) {
        if(info->modules[m].type == ModuleDescriptor::TYPE_RAMDISK_IMAGE)
            fontBuffer = (uint32*)info->modules[m].buffer;
    }
    Terminal::Init(fontBuffer, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth);

    Terminal::Clear();

    printf("Integer: %i\nHex: %X\nPadded Hex: %x\nString: %s\n", 42, 0xFF00FF, 0xBAD, "Some String");

    while(true);

}