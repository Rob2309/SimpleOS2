#pragma once

#include "./internal/ELFProgramInfo.h"

extern ELFProgramInfo* g_ProgInfo;

void CreateThread(int (*func)());