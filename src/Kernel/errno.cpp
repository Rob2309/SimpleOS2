#include "errno.h"

const char* ErrorToString(int64 error) {
    switch(error) {
    case OK: return "OK";
    case ErrorFileNotFound: return "File not found";
    case ErrorInvalidBuffer: return "Invalid user buffer";
    case ErrorInvalidFD: return "Invalid file descriptor";
    case ErrorInvalidFileSystem: return "Invalid file system id";
    case ErrorInvalidPath: return "Invalid path";
    case ErrorPermissionDenied: return "Permission denied";
    case ErrorInvalidDevice: return "Invalid device";
    case ErrorFileExists: return "File already exists";
    case ErrorSeekOffsetOOB: return "Seek offset out of bounds";
    case ErrorEncounteredSymlink: return "Encountered Symlink";
    default: return "unknown error";
    }
}