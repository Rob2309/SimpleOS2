#include "Syscall.h"

void Print(const char* str)
{
    __asm__ __volatile__ (
        "int $0x80"
        : : "a"(Syscall::FunctionPrint), "D"(str)
    );
}

extern "C" void main()
{

    Print("Hello World from the Test Program\n");

    while(true);

}