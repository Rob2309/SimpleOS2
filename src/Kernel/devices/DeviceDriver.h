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
    /**
     * Returns the ID with which this Driver can be retrieved from DeviceDriverRegistry::GetDriver
     **/
    uint64 GetDriverID() const { return m_ID; }

private:
    Type m_Type;
    uint64 m_ID;
};

class CharDeviceDriver : public DeviceDriver {
public:
    CharDeviceDriver();

    virtual uint64 Read(uint64 subID, void* buffer, uint64 bufferSize) = 0;
    virtual uint64 Write(uint64 subID, const void* buffer, uint64 bufferSize) = 0;
};

class BlockDeviceDriver : public DeviceDriver {
public:
    BlockDeviceDriver();

    virtual uint64 GetBlockSize(uint64 subID) const = 0;

    virtual void ReadBlock(uint64 subID, uint64 startBlock, uint64 numBlocks, void* buffer) = 0;
    virtual void WriteBlock(uint64 subID, uint64 startBlock, uint64 numBlocks, const void* buffer) = 0;
};

class DeviceDriverRegistry {
public:
    /**
     * Registers a Driver to the Registry.
     * @returns the ID with which the given driver can be retrieved again.
     **/
    static uint64 RegisterDriver(DeviceDriver* driver);
    /**
     * Removes a Driver from the Registry.
     **/
    static void UnregisterDriver(uint64 id);
    /**
     * Retrieves a Driver from the Registry
     **/
    static DeviceDriver* GetDriver(uint64 id);
};