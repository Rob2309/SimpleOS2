#include "DeviceDriver.h"

#include "ktl/AnchorList.h"
#include "ktl/new.h"
#include "scheduler/Scheduler.h"
#include "locks/QueueLock.h"
#include "fs/VFS.h"
#include "klib/string.h"

DeviceDriver::DeviceDriver(Type type, const char* name) 
    : m_Type(type), m_Name(name)
{
    m_ID = DeviceDriverRegistry::RegisterDriver(this);
}

CharDeviceDriver::CharDeviceDriver(const char* name) 
    : DeviceDriver(TYPE_CHAR, name)
{ }

BlockDeviceDriver::BlockDeviceDriver(const char* name)
    : DeviceDriver(TYPE_BLOCK, name)
{ }


struct CachedBlock {
    uint64 subID;
    uint64 blockID;
    uint64 refCount;
    QueueLock dataLock;
    char data[];
};

static bool GetCachedBlock(uint64 subID, uint64 blockID, uint64 blockSize, ktl::vector<CachedBlock*>& cache, CachedBlock** block) {
    for(CachedBlock* cb : cache) {
        if(cb->subID == subID && cb->blockID == blockID) {
            cb->refCount++;
            *block = cb;
            return true;
        }
    }

    char* buffer = new char[sizeof(CachedBlock) + blockSize];
    CachedBlock* cb = new(buffer) CachedBlock();
    cb->subID = subID;
    cb->blockID = blockID;
    cb->refCount = 1;
    cache.push_back(cb);
    *block = cb;
    return false;
}
static void ReleaseCachedBlock(CachedBlock* cb, ktl::vector<CachedBlock*>& cache) {
    cb->refCount--;
    if(cb->refCount == 0) {
        // TODO: Cache clean logic
    }
}

uint64 BlockDeviceDriver::GetData(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) {
    char* realBuffer = (char*)buffer;

    uint64 startBlock = pos / GetBlockSize(subID);
    uint64 endBlock = (pos + bufferSize) / GetBlockSize(subID);
    uint64 numBlocks = endBlock - startBlock + 1;

    for(uint64 b = 0; b < numBlocks; b++) {
        uint64 blockID = startBlock + b;
        
        m_CacheLock.Spinlock();
        CachedBlock* cb;
        if(!GetCachedBlock(subID, blockID, GetBlockSize(subID), m_Cache, &cb)) {
            m_CacheLock.Unlock();
            cb->dataLock.Lock();

            Atomic<uint64> finished;
            finished = 0;
            ScheduleOperation(subID, blockID, 1, false, cb->data, &finished);
            while(finished.Read() == 0) ;
                // TODO: Yield
        } else {
            m_CacheLock.Unlock();
            cb->dataLock.Lock();
        }

        uint64 offs = pos - (blockID * GetBlockSize(subID));
        uint64 rem = GetBlockSize(subID) - offs;
        if(rem > bufferSize)
            rem = bufferSize;

        if(!kmemcpy_usersafe(realBuffer, cb->data + offs, rem)) {
            cb->dataLock.Unlock();
            m_CacheLock.Spinlock();
            ReleaseCachedBlock(cb, m_Cache);
            m_CacheLock.Unlock();
            return ErrorInvalidBuffer;
        }
        cb->dataLock.Unlock();
        m_CacheLock.Spinlock();
        ReleaseCachedBlock(cb, m_Cache);
        m_CacheLock.Unlock();

        pos += rem;
        realBuffer += rem;
        bufferSize -= rem;
    }

    return 0;
}
uint64 BlockDeviceDriver::SetData(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) {
    char* realBuffer = (char*)buffer;

    uint64 startBlock = pos / GetBlockSize(subID);
    uint64 endBlock = (pos + bufferSize) / GetBlockSize(subID);
    uint64 numBlocks = endBlock - startBlock + 1;

    // TODO: no-read optimization of offs == 0

    for(uint64 b = 0; b < numBlocks; b++) {
        uint64 blockID = startBlock + b;
        
        m_CacheLock.Spinlock();
        CachedBlock* cb;
        if(!GetCachedBlock(subID, blockID, GetBlockSize(subID), m_Cache, &cb)) {
            m_CacheLock.Unlock();
            cb->dataLock.Lock();

            Atomic<uint64> finished;
            finished = 0;
            ScheduleOperation(subID, blockID, 1, false, cb->data, &finished);
            while(finished.Read() == 0) ;
                // TODO: Yield
        } else {
            m_CacheLock.Unlock();
            cb->dataLock.Lock();
        }

        uint64 offs = pos - (blockID * GetBlockSize(subID));
        uint64 rem = GetBlockSize(subID) - offs;
        if(rem > bufferSize)
            rem = bufferSize;

        if(!kmemcpy_usersafe(cb->data + offs, realBuffer, rem)) {
            cb->dataLock.Unlock();
            m_CacheLock.Spinlock();
            ReleaseCachedBlock(cb, m_Cache);
            m_CacheLock.Unlock();
            return ErrorInvalidBuffer;
        }
        cb->dataLock.Unlock();
        m_CacheLock.Spinlock();
        ReleaseCachedBlock(cb, m_Cache);
        m_CacheLock.Unlock();

        pos += rem;
        realBuffer += rem;
        bufferSize -= rem;
    }

    return 0;
}

static StickyLock g_DriverLock;
static uint64 g_DriverIDCounter = 0;
static ktl::AnchorList<DeviceDriver, &DeviceDriver::m_Anchor> g_Drivers;

uint64 DeviceDriverRegistry::RegisterDriver(DeviceDriver* driver) {
    g_DriverLock.Spinlock();
    uint64 res = g_DriverIDCounter++;
    g_Drivers.push_back(driver);
    g_DriverLock.Unlock();
    return res;
}
void DeviceDriverRegistry::UnregisterDriver(DeviceDriver* driver) {
    g_DriverLock.Spinlock();
    g_Drivers.erase(driver);
    g_DriverLock.Unlock();
}
DeviceDriver* DeviceDriverRegistry::GetDriver(uint64 id) {
    g_DriverLock.Spinlock();
    for(DeviceDriver& driver : g_Drivers) {
        if(driver.GetDriverID() == id) {
            g_DriverLock.Unlock();
            return &driver;
        }
    }
    g_DriverLock.Unlock();
    return nullptr;
}
DeviceDriver* DeviceDriverRegistry::GetDriver(const char* name) {
    g_DriverLock.Spinlock();
    for(DeviceDriver& driver : g_Drivers) {
        if(kstrcmp(driver.GetName(), name) == 0) {
            g_DriverLock.Unlock();
            return &driver;
        }
    }
    g_DriverLock.Unlock();
    return nullptr;
}