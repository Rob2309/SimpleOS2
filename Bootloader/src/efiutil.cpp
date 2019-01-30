#include "efiutil.h"

namespace EFIUtil {

    EFI_SYSTEM_TABLE* SystemTable;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;

    void WaitForKey()
    {
        SystemTable->ConIn->Reset(SystemTable->ConIn, false);
        
        EFI_INPUT_KEY key;
        
        EFI_STATUS ret;

        while((ret = SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key)) == EFI_NOT_READY)
        ;
    }

}