#include "RamDevice.h"

#include "memutil.h"

RamDevice::RamDevice(const char* name, void* buf, uint64 size)
    : Device(name), m_Buffer((char*)buf), m_Size(size), m_Pos(0)
{ }

void RamDevice::SetPos(uint64 pos) {
    m_Pos = pos;
}

uint64 RamDevice::Read(void* buffer, uint64 bufferSize) {
    if(m_Pos == m_Size)
        return 0;

    uint64 rem = m_Size - m_Pos;
    if(bufferSize < rem)
        rem = bufferSize;

    memcpy(buffer, m_Buffer + m_Pos, rem);
    m_Pos += rem;
    return rem;
}

void RamDevice::Write(void* buffer, uint64 bufferSize) {
    if(m_Pos == m_Size)
        return;

    uint64 rem = m_Size - m_Pos;
    if(bufferSize < rem)
        rem = bufferSize;

    memcpy(m_Buffer + m_Pos, buffer, rem);
    m_Pos += rem;
    return;
}