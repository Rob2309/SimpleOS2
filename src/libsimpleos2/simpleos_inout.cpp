#include "simpleos_inout.h"

#include "simpleos_vfs.h"
#include "simpleos_string.h"

void puts(const char* str) {
    auto l = strlen(str);
    write(stdoutfd, str, l);
}