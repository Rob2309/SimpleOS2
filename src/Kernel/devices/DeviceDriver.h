#pragma once

#include "types.h"

class DeviceDriver {
public:
    enum Type {
        TYPE_CHAR,
        TYPE_BLOCK,
    };

public:
    DeviceDriver(Type type);

    Type GetType() const { return m_Type; }
    uint64 GetDriverID() const { return m_ID; }

    virtual uint64 Read(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) = 0;
    virtual uint64 Write(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) = 0;

private:
    Type m_Type;
    uint64 m_ID;
};

class CharDeviceDriver : public DeviceDriver {
public:
    CharDeviceDriver();
};

class BlockDeviceDriver : public DeviceDriver {
public:
    BlockDeviceDriver();

    virtual uint64 GetBlockSize(uint64 subID) const = 0;
};

class DeviceDriverRegistry {
public:
    static uint64 RegisterDriver(DeviceDriver* driver);
    static void UnregisterDriver(uint64 id);
    static DeviceDriver* GetDriver(uint64 id);
};