#include "Time.h"

#include "arch/MSR.h"
#include "arch/CPU.h"
#include "arch/port.h"

#include "klib/stdio.h"

namespace Time {

    static uint64 g_TSCTicksPerMilli;

    static void CalibrateTSC() {
        uint64 ticksPerMS = 0;
        for(int i = 0; i < 5; i++) {
            Port::OutByte(0x43, 0x30);

            constexpr uint16 pitTickInitCount = 39375; // 33 ms
            Port::OutByte(0x40, pitTickInitCount & 0xFF);
            Port::OutByte(0x40, (pitTickInitCount >> 8) & 0xFF);

            uint64 tscStart;
            uint64 edx, eax;
            __asm__ __volatile__ (
                "mfence;"
                "rdtsc;"
                "lfence"
                : "=d"(edx), "=a"(eax)
            );
            tscStart = (edx << 32) | eax;

            uint16 pitCurrentCount = 0;
            while(pitCurrentCount <= pitTickInitCount)
            {
                Port::OutByte(0x43, 0);
                pitCurrentCount = (uint16)Port::InByte(0x40) | ((uint16)Port::InByte(0x40) << 8);
            }

            uint64 tscEnd;
            __asm__ __volatile__ (
                "mfence;"
                "rdtsc;"
                "lfence"
                : "=d"(edx), "=a"(eax)
            );
            tscEnd = (edx << 32) | eax;

            uint64 elapsed = tscEnd - tscStart;
            ticksPerMS += elapsed / 33;
        }

        ticksPerMS /= 5;
        klog_info("Time", "TSC runs at %i kHz", ticksPerMS);
        g_TSCTicksPerMilli = ticksPerMS;
    }

    bool Init() {
        uint64 eax, ebx, ecx, edx;
        CPU::CPUID(1, 0, eax, ebx, ecx, edx);
        if((edx & (1 << 4)) == 0) {
            klog_fatal("Time", "CPU Time stamp counter not supported");
            return false;
        }

        CPU::CPUID(0x80000001, 0, eax, ebx, ecx, edx);
        if((edx & (1 << 27)) == 0) {
            klog_fatal("Time", "RDTSCP instruction not supported");
            return false;
        }

        CPU::CPUID(0x80000007, 0, eax, ebx, ecx, edx);
        if((edx & (1 << 8)) == 0) {
            klog_fatal("Time", "TSC is not invariant");
            return false;
        }

        CalibrateTSC();

        return true;
    }

    uint64 GetTSCTicksPerMilli() {
        return g_TSCTicksPerMilli;
    }

    uint64 GetTSC() {
        uint64 edx, eax;
        __asm__ __volatile__ (
            "rdtsc"
            : "=d"(edx), "=a"(eax)
        );
        return ((edx << 32) & 0xFFFFFFFF00000000) | (eax & 0xFFFFFFFF);
    }

    static uint8 ReadRTCReg(uint8 reg) {
        Port::OutByte(0x70, reg);
        return Port::InByte(0x71);
    }

    #define CONVERT_BCD(var) var = (var & 0x0F) + (var >> 4) * 10

    void GetRTC(DateTime* dt) {
        uint8 lastSecond = ReadRTCReg(0x00);
        uint8 lastMinute = ReadRTCReg(0x02);
        uint8 lastHour = ReadRTCReg(0x04);
        uint8 lastDOM = ReadRTCReg(0x07);
        uint8 lastMonth = ReadRTCReg(0x08);
        uint8 lastYear = ReadRTCReg(0x09);

        while(true) {
            uint8 second = ReadRTCReg(0x00);
            uint8 minute = ReadRTCReg(0x02);
            uint8 hour = ReadRTCReg(0x04);
            uint8 dayOfMonth = ReadRTCReg(0x07);
            uint8 month = ReadRTCReg(0x08);
            uint8 year = ReadRTCReg(0x09);

            if(lastSecond == second && lastMinute == minute && lastHour == hour && lastDOM == dayOfMonth && lastMonth == month && lastYear == year) {
                break;
            }

            lastSecond = second;
            lastMinute = minute;
            lastHour = hour;
            lastDOM = dayOfMonth;
            lastMonth = month;
            lastYear = year;
        }

        uint8 format = ReadRTCReg(0x0B);
        if((format & 0x04) == 0) {  // BCD mode
            CONVERT_BCD(lastSecond);
            CONVERT_BCD(lastMinute);
            CONVERT_BCD(lastHour);
            CONVERT_BCD(lastDOM);
            CONVERT_BCD(lastMonth);
            CONVERT_BCD(lastYear);
        }
        if((format & 0x02) == 0 && (lastHour & 0x80)) {
            lastHour = (lastHour & 0x7F) + 12;
        }

        dt->seconds = lastSecond;
        dt->minutes = lastMinute;
        dt->hours = lastHour;
        dt->dayOfMonth = lastDOM;
        dt->month = lastMonth;
        dt->year = lastYear;
    }

}