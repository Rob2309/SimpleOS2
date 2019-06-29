#pragma once

#include "types.h"
#include "atomic/Atomics.h"
#include "ktl/vector.h"
#include "locks/StickyLock.h"

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

public:
    DeviceDriver* next;
    DeviceDriver* prev;

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

struct CachedBlock;

class BlockDeviceDriver : public DeviceDriver {
public:
    BlockDeviceDriver();

    void GetData(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize);
    void SetData(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize);

    virtual uint64 GetBlockSize(uint64 subID) const = 0;

protected:
    virtual void ScheduleOperation(uint64 subID, uint64 startBlock, uint64 numBlocks, bool write, void* buffer, Atomic<uint64>* finishFlag) = 0;

private:
    StickyLock m_CacheLock;
    ktl::vector<CachedBlock*> m_Cache;
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
    static void UnregisterDriver(DeviceDriver* driver);
    /**
     * Retrieves a Driver from the Registry
     **/
    static DeviceDriver* GetDriver(uint64 id);
};