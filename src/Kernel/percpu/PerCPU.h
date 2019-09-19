#pragma once

#include "types.h"
#include "multicore/SMP.h"

namespace _PerCpuInternal {
    void* GetAddress(uint64 coreID, void* val);
}

template<typename T>
class PerCpuVar {
public:

    T& Get() {
        auto core = SMP::GetLogicalCoreID();
        auto addr = _PerCpuInternal::GetAddress(core, &m_Val);
        return *(T*)addr;
    }

    T& Get(uint64 coreID) {
        auto addr = _PerCpuInternal::GetAddress(coreID, &m_Val);
        return *(T*)addr;
    }

private:
    T m_Val;
};

#define DECLARE_PER_CPU(type, name) __attribute__((section(".kpercpu"))) PerCpuVar<type> name