#include "Device.h"

#include "ArrayList.h"
#include "VFS.h"
#include "conio.h"

static ArrayList<Device*> g_Devices;

Device* Device::GetByID(uint64 id) {
    return g_Devices[id];
}

Device::Device(const char* name) {
    m_DevID = g_Devices.size();
    g_Devices.push_back(this);

    if(!VFS::CreateDeviceFile("/dev", name, m_DevID))
        printf("Failed to create device file for %s\n", name);
}