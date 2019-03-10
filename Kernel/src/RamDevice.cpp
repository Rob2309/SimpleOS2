#include "RamDevice.h"

#include "memutil.h"

RamDevice::RamDevice(const char* name, void* buf, uint64 size)
    : Device(name), m_Buffer((char*)buf), m_Size(size)
{ }

uint64 RamDevice::Read(uint64 pos, void* buffer, uint64 bufferSize) {
    if(pos == m_Size)
        return 0;

    uint64 rem = m_Size - pos;
    if(bufferSize < rem)
        rem = bufferSize;

    memcpy(buffer, m_Buffer + pos, rem);
    return rem;
}

void RamDevice::Write(uint64 pos, void* buffer, uint64 bufferSize) {
    if(pos == m_Size)
        return;

    uint64 rem = m_Size - pos;
    if(bufferSize < rem)
        rem = bufferSize;

    memcpy(m_Buffer + pos, buffer, rem);
    return;
}