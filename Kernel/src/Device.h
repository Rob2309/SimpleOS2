#pragma once

#include "types.h"

class Device 
{
public:
    static Device* GetByID(uint64 id);

public:
    Device(const char* name);
    
    virtual void SetPos(uint64 pos) = 0;
    virtual uint64 Read(void* buffer, uint64 bufferSize) = 0;
    virtual void Write(void* buffer, uint64 bufferSize) = 0;

    uint64 GetDeviceID() const { return m_DevID; }

private:
    uint64 m_DevID;
};