#pragma once

#include "types.h"

namespace ISRNumbers
{
    constexpr uint8 ExceptionDiv0 = 0;
    constexpr uint8 ExceptionDebug = 1;
    constexpr uint8 ExceptionNMI = 2;
    constexpr uint8 ExceptionBreakpoint = 3;
    constexpr uint8 ExceptionOverflow = 4;
    constexpr uint8 ExceptionBoundRangeExceeded = 5;
    constexpr uint8 ExceptionInvalidOpcode = 6;
    constexpr uint8 ExceptionDeviceUnavailable = 7;
    constexpr uint8 ExceptionDoubleFault = 8;
    constexpr uint8 ExceptionCoprocesssorSegmentOverrun = 9;
    constexpr uint8 ExceptionInvalidTSS = 10;
    constexpr uint8 ExceptionSegmentNotPresent = 11;
    constexpr uint8 ExceptionStackSegmentNotPresent = 12;
    constexpr uint8 ExceptionGPFault = 13;
    constexpr uint8 ExceptionPageFault = 14;
    constexpr uint8 ExceptionFPException = 16;
    constexpr uint8 ExceptionAlignmentCheck = 17;
    constexpr uint8 ExceptionMachineCheck = 18;
    constexpr uint8 ExceptionSIMDFP = 19;
    constexpr uint8 ExceptionVirtualization = 20;
    constexpr uint8 ExceptionSecurity = 30;

    constexpr uint8 APICTimer = 100; // Interrupt of Local APIC Timer
    constexpr uint8 APICError = 101; // Interrupt if Local APIC throws an error

    constexpr uint8 IPIPagingSync = 120; // Called by memory manager to keep page tables in sync across multiple cores
    
    constexpr uint8 APICSpurious = 255; // Spurious Local APIC Interrupt
}