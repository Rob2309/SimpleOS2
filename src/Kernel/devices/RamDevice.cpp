#include "RamDevice.h"

#include "klib/memory.h"

RamDevice::RamDevice(const char* name, void* buf, uint64 size)
    : Device(name), m_Buffer((char*)buf), m_Size(size)
{ }

uint64 RamDevice::Read(uint64 pos, void* buffer, uint64 bufferSize) {
    if(pos == m_Size)
        return 0;

    uint64 rem = m_Size - pos;
    if(bufferSize < rem)
        rem = bufferSize;

    kmemcpy(buffer, m_Buffer + pos, rem);
    return rem;
}

uint64 RamDevice::Write(uint64 pos, void* buffer, uint64 bufferSize) {
    if(pos == m_Size)
        return 0;

    uint64 rem = m_Size - pos;
    if(bufferSize < rem)
        rem = bufferSize;

    kmemcpy(m_Buffer + pos, buffer, rem);
    return rem;
}