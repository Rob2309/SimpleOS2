#pragma once

#include "types.h"

constexpr int64 OK = 0;
constexpr int64 ErrorFileNotFound = -4;
constexpr int64 ErrorPermissionDenied = -5;
constexpr int64 ErrorInvalidFD = -6;
constexpr int64 ErrorInvalidBuffer = -7;
constexpr int64 ErrorInvalidPath = -8;
constexpr int64 ErrorInvalidFileSystem = -9;
constexpr int64 ErrorInvalidDevice = -10;
constexpr int64 ErrorFileExists = -11;
constexpr int64 ErrorSeekOffsetOOB = -12;
constexpr int64 ErrorEncounteredSymlink = -13;

constexpr int64 ErrorThreadNotFound = -100;

constexpr int64 ErrorInterrupted = -200;

const char* ErrorToString(int64 error);