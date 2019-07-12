#pragma once

#include "../ExecHandler.h"

class ELFExecHandler : public ExecHandler {
public:
    static void Init();

public:
    bool CheckAndPrepare(uint8* buffer, uint64 bufferSize, uint64 pml4Entry, IDT::Registers* regs) override;
};