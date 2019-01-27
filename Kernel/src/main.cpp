
#include <KernelHeader.h>

int g_Ret = 17;

int Test() {
    g_Ret += 5;
    return g_Ret;
}

extern "C" int __attribute__((ms_abi)) main(KernelHeader* info) {
    info->printf("Hello from the Kernel\n");

    return Test();
}