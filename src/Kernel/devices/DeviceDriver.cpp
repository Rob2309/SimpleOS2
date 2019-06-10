#include "DeviceDriver.h"

#include "Mutex.h"
#include "ktl/vector.h"
#include "ktl/new.h"
#include "scheduler/Scheduler.h"

DeviceDriver::DeviceDriver(Type type) 
    : m_Type(type)
{
    m_ID = DeviceDriverRegistry::RegisterDriver(this);
}

CharDeviceDriver::CharDeviceDriver() 
    : DeviceDriver(TYPE_CHAR)
{ }

BlockDeviceDriver::BlockDeviceDriver()
    : DeviceDriver(TYPE_BLOCK)
{ }


struct CachedBlock {
    uint64 subID;
    uint64 blockID;
    uint64 refCount;
    Mutex dataLock;
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

void BlockDeviceDriver::GetData(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) {
    uint64 startBlock = pos / GetBlockSize(subID);
    uint64 endBlock = (pos + bufferSize) / GetBlockSize(subID);
    uint64 numBlocks = endBlock - startBlock + 1;

    for(uint64 b = 0; b < numBlocks; b++) {
        uint64 blockID = startBlock + b;
        
        m_CacheLock.SpinLock();
        CachedBlock* cb;
        if(!GetCachedBlock(subID, blockID, GetBlockSize(subID), m_Cache, &cb)) {
            cb->dataLock.SpinLock();
            m_CacheLock.Unlock();

            Atomic<uint64> finished;
            finished = 0;
            ScheduleOperation(subID, blockID, 1, false, cb->data, &finished);
            while(finished.Read() == 0) ;
                // TODO: Yield
        } else {
            m_CacheLock.Unlock();
            cb->dataLock.SpinLock();
        }

        uint64 offs = pos - (blockID * GetBlockSize(subID));
        uint64 rem = GetBlockSize(subID) - offs;
        if(rem > bufferSize)
            rem = bufferSize;

        kmemcpy(buffer, cb->data + offs, rem);
        cb->dataLock.Unlock();
        m_CacheLock.SpinLock();
        ReleaseCachedBlock(cb, m_Cache);
        m_CacheLock.Unlock();

        pos += rem;
        buffer += rem;
        bufferSize -= rem;
    }
}
void BlockDeviceDriver::SetData(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) {
    uint64 startBlock = pos / GetBlockSize(subID);
    uint64 endBlock = (pos + bufferSize) / GetBlockSize(subID);
    uint64 numBlocks = endBlock - startBlock + 1;

    // TODO: no-read optimization of offs == 0

    for(uint64 b = 0; b < numBlocks; b++) {
        uint64 blockID = startBlock + b;
        
        m_CacheLock.SpinLock();
        CachedBlock* cb;
        if(!GetCachedBlock(subID, blockID, GetBlockSize(subID), m_Cache, &cb)) {
            cb->dataLock.SpinLock();
            m_CacheLock.Unlock();

            Atomic<uint64> finished;
            finished = 0;
            ScheduleOperation(subID, blockID, 1, false, cb->data, &finished);
            while(finished.Read() == 0) ;
                // TODO: Yield
        } else {
            m_CacheLock.Unlock();
            cb->dataLock.SpinLock();
        }

        uint64 offs = pos - (blockID * GetBlockSize(subID));
        uint64 rem = GetBlockSize(subID) - offs;
        if(rem > bufferSize)
            rem = bufferSize;

        kmemcpy(cb->data + offs, buffer, rem);
        cb->dataLock.Unlock();
        m_CacheLock.SpinLock();
        ReleaseCachedBlock(cb, m_Cache);
        m_CacheLock.Unlock();

        pos += rem;
        buffer += rem;
        bufferSize -= rem;
    }
}

static Mutex g_DriverLock;
static ktl::vector<DeviceDriver*> g_Drivers;

uint64 DeviceDriverRegistry::RegisterDriver(DeviceDriver* driver) {
    g_DriverLock.SpinLock();
    uint64 res = g_Drivers.size();
    g_Drivers.push_back(driver);
    g_DriverLock.Unlock();
    return res;
}
void DeviceDriverRegistry::UnregisterDriver(uint64 id) {
    g_DriverLock.SpinLock();
    g_Drivers[id] = nullptr;
    g_DriverLock.Unlock();
}
DeviceDriver* DeviceDriverRegistry::GetDriver(uint64 id) {
    g_DriverLock.SpinLock();
    DeviceDriver* res = g_Drivers[id];
    g_DriverLock.Unlock();
    return res;
}