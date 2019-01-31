
#include <KernelHeader.h>

const char* g_TypeDescs[] = {
    "EfiReservedMemoryType",
    "EfiLoaderCode",
    "EfiLoaderData",
    "EfiBootServicesCode",
    "EfiBootServicesData",
    "EfiRuntimeServicesCode",
    "EfiRuntimeServicesData",
    "EfiConventionalMemory",
    "EfiUnusableMemory",
    "EfiACPIReclaimMemory",
    "EfiACPIMemoryNVS",
    "EfiMemoryMappedIO",
    "EfiMemoryMappedIOPortSpace",
    "EfiPalCode",
    "EfiMaxMemoryType"
};

int g_Ret = 17;

int Test() {
    g_Ret += 5;
    return g_Ret;
}

extern "C" void __attribute__((ms_abi)) main(KernelHeader* info) {
    
    for(int y = 100; y < info->screenHeight - 100; y++) {
        for(int x = 100; x < info->screenWidth - 100; x++) {
            info->screenBuffer[x + y * info->screenScanlineWidth] = 0xFF00FF;
        }
    }

    while(true);

}