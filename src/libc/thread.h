#pragma once

#include "ELFProgramInfo.h"

extern ELFProgramInfo* g_ProgInfo;

void CreateThread(int (*func)());