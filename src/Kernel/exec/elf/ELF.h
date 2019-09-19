#pragma once

#include "../ExecHandler.h"

class ELFExecHandler : public ExecHandler {
public:
    bool CheckAndPrepare(uint8* buffer, uint64 bufferSize, uint64 pml4Entry, IDT::Registers* regs, int argc, char** argv) override;
};