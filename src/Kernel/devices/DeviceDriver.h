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

    /**
     * Reads from a specific device that belongs to this driver.
     * @param subID An ID that identifies the device to read from.
     * @param pos The offset to start reading from, has to be a multiple of the block size if this device is a block device.
     * @param bufferSize The size of the buffer to read into, has to be a multiple of the block size if this device is a block device.
     **/
    virtual uint64 Read(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) = 0;
    /**
     * Writes to a specific device that belongs to this driver.
     * @param subID An ID that identifies the device to write to.
     * @param pos The offset to start writing to, has to be a multiple of the block size if this device is a block device.
     * @param bufferSize The size of the buffer to write data from, has to be a multiple of the block size if this device is a block device.
     **/
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