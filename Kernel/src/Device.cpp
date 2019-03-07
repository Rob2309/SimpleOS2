#include "Device.h"

#include "ArrayList.h"
#include "VFS.h"
#include "conio.h"

static ArrayList<Device*>* g_Devices = nullptr;

Device* Device::GetByID(uint64 id) {
    return (*g_Devices)[id];
}

Device::Device(const char* name) {
    if(g_Devices == nullptr)
        g_Devices = new ArrayList<Device*>();

    m_DevID = g_Devices->size();
    g_Devices->push_back(this);

    if(!VFS::CreateDeviceFile("/dev", name, m_DevID))
        printf("Failed to create device file for %s\n", name);
}