
#include <KernelHeader.h>

/*const char* g_TypeDescs[] = {
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
};*/

int g_Ret = 17;

int Test() {
    g_Ret += 5;
    return g_Ret;
}

extern "C" int __attribute__((ms_abi)) main(KernelHeader* info) {
    info->printf("Physical            Virtual               Size                  Type\n");
  //info->printf("0x0000000000000000  0x0000000000000000    0x0000000000000000    ");

    MemoryDescriptor* entry = info->memMap;
    for(int i = 0; i < info->memMapLength; i++) {
        //info->printf("0x%x  0x%x    0x%x    %i\n", entry->physicalStart, entry->virtualStart, entry->numPages * 4096, entry->type);

        entry = (MemoryDescriptor*)((char*)entry + info->memMapDescriptorSize);
    }

    return Test();
}