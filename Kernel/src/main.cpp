
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

extern "C" int __attribute__((ms_abi)) main(KernelHeader* info) {
    return Test();
}