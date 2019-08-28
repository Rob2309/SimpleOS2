#include "devices/DeviceDriver.h"

namespace PS2 {

    class PS2Driver : public CharDeviceDriver {
    public:
        static constexpr uint64 DeviceKeyboard = 0;

    public:
        PS2Driver();

        uint64 Read(uint64 subID, void* buffer, uint64 bufferSize) override;
        uint64 Write(uint64 subID, const void* buffer, uint64 bufferSize) override;
    };

}