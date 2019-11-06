#include "simpleos_errno.h"

__thread int64 errno;

const char* ErrorToString(int64 error) {
    switch(error) {
    case OK: return "OK";
    case ErrorFileNotFound: return "File not found";
    case ErrorPermissionDenied: return "Permission denied";
    case ErrorInvalidFD: return "Invalid File descriptor";
    case ErrorInvalidBuffer: return "Invalid user buffer";
    case ErrorInvalidPath: return "Invalid path";
    case ErrorInvalidFileSystem: return "Invalid Filesystem";
    case ErrorInvalidDevice: return "Invalid device";
    case ErrorFileExists: return "File exists";
    case ErrorSeekOffsetOOB: return "Seek offset out of bounds";
    case ErrorEncounteredSymlink: return "Encountered symlink";
    case ErrorAddressNotPageAligned: return "Address not page aligned";
    case ErrorPathTooLong: return "Path too long";
    case ErrorHardlinkToFolder: return "Hardlink to folder not allowed";
    case ErrorHardlinkToDifferentFS: return "Hardlink to different filesystem not allowed";
    case ErrorDeleteMountPoint: return "Deleting mountpoint not allowed";
    case ErrorFolderNotEmpty: return "Folder not empty";
    case ErrorNotAFolder: return "Not a folder";
    case ErrorFolderAlreadyMounted: return "Folder is already mounted";
    case ErrorNotABlockDevice: return "Not a block device";
    case ErrorUnmountRoot: return "Unmounting root filesystem not allowed";
    case ErrorMountPointBusy: return "MountPoint is busy";
    case ErrorOpenFolder: return "Opening a folder is not allowed";
    case ErrorNotADevice: return "Not a device";
    
    case ErrorThreadNotFound: return "Thread not found";
    case ErrorDetachSubThread: return "Detaching a subthread is not allowed";
    case ErrorThreadNotExited: return "Thread has not exited yet";

    case ErrorInterrupted: return "Interrupted";

    default: return "unknown error";
}