#pragma once

#include "types.h"

namespace VFS {

    struct Permissions {
        static constexpr uint8 Read = 0x1;
        static constexpr uint8 Write = 0x2;

        uint8 ownerPermissions;
        uint8 groupPermissions;
        uint8 otherPermissions;
    };
    
}