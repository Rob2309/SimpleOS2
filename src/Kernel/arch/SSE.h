#pragma once

#include "types.h"

namespace SSE {
    /**
     * Checks if the required SSE functions are supported and initializes the basic SSE structures.
     * Returns true if every required SSE extension is supported, false otherwise.
     **/
    bool InitBootCore();
    void InitCore();

    /**
     * Returns the buffer size needed to store the state of the Floating point unit, SSE registers etc.
     **/
    uint64 GetFPUBlockSize();
    /**
     * Saves the current FPU state into the given buffer.
     **/
    void SaveFPUBlock(char* buffer);
    /**
     * Restores the current FPU state from the given buffer.
     **/
    void RestoreFPUBlock(char* buffer);
    /**
     * Initializes the given buffer to the default FPU state.
     * Buffer has to be zeroed out before calling this function.
     **/
    void InitFPUBlock(char* buffer);

}