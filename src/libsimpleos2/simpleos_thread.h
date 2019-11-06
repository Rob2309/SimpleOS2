#pragma once

#include "./internal/ELFProgramInfo.h"

extern ELFProgramInfo* g_ProgInfo;

/**
 * Creates a new thread with func as entry point.
 **/
void CreateThread(int (*func)());