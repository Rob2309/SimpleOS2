#include "DeviceDriver.h"

#include "Mutex.h"
#include "stl/ArrayList.h"

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




static Mutex g_DriverLock;
static ArrayList<DeviceDriver*> g_Drivers;

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