#pragma once

#include "types.h"
#include "interrupts/IDT.h"

class ExecHandler {
public:
    virtual bool CheckAndPrepare(uint8* buffer, uint64 bufferSize, uint64 pml4Entry, IDT::Registers* regs, int argc, char** argv) = 0;

public:
    ExecHandler* next;
    ExecHandler* prev;
};

class ExecHandlerRegistry {
public:
    static void RegisterHandler(ExecHandler* handler);
    static void UnregisterHandler(ExecHandler* handler);

    static bool Prepare(uint8* buffer, uint64 bufferSize, uint64 pml4Entry, IDT::Registers* regs, int argc, char** argv);
};