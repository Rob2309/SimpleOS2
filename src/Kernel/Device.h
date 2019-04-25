#pragma once

#include "types.h"

class Device 
{
public:
    static Device* GetByID(uint64 id);

public:
    Device(const char* name);
    
    virtual uint64 Read(uint64 pos, void* buffer, uint64 bufferSize) = 0;
    virtual uint64 Write(uint64 pos, void* buffer, uint64 bufferSize) = 0;

    uint64 GetDeviceID() const { return m_DevID; }

private:
    uint64 m_DevID;
};