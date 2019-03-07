#include "VFS.h"

#include "ArrayList.h"
#include "string.h"
#include "conio.h"

namespace VFS {

    struct File
    {
        static constexpr uint64 StatusOpenedForRead = 0x1;
        static constexpr uint64 StatusOpenedForWrite = 0x2;

        FileType type;

        union {
            struct {
                uint64 size;
                char* data;
            } file;
            struct {
                uint64 numFiles;
                File* files;
            } directory;
        };

        uint64 status;

        char name[50];
    };
    struct FileDescriptor
    {
        uint64 id;
        uint64 mode;
        
        uint64 pos;
        File* file;
    };

    static File g_RootFile;
    
    static uint64 g_FileDescCounter = 1;
    static ArrayList<FileDescriptor>* g_FileDescriptors;

    void Init()
    {
        g_RootFile.type = FILETYPE_DIRECTORY;
        g_RootFile.directory.numFiles = 0;
        g_RootFile.directory.files = nullptr;
        g_RootFile.status = 0;
        memset(g_RootFile.name, 0, sizeof(g_RootFile.name));

        g_FileDescriptors = new ArrayList<FileDescriptor>();
    }

    static File* FindFile(File* folder, const char* path)
    {
        if(path[0] == '\0') {
            return folder;
        }
        if(folder->type != FILETYPE_DIRECTORY) {
            return nullptr;
        }

        uint64 end = 0;
        while(true) {
            if(path[end] == '/' || path[end] == '\0') {
                File* ret = nullptr;
                for(uint64 i = 0; i < folder->directory.numFiles; i++) {
                    if(strcmp(path, 0, end, folder->directory.files[i].name) == 0) {
                        ret = &folder->directory.files[i];
                        break;
                    }
                }
                if(ret == nullptr)
                    return nullptr;
                
                if(path[end] == '\0')
                    return ret;
                else 
                    return FindFile(ret, &path[end + 1]);
            } else {
                end++;
            }
        }
    }

    bool CreateFile(const char* path, const char* name, FileType type)
    {
        File* folder = FindFile(&g_RootFile, path + 1);
        if(folder == nullptr)
            return false;
        if(!folder->type == FILETYPE_DIRECTORY)
            return false;

        File newFile;
        newFile.type = type;
        newFile.status = 0;
        memcpy(newFile.name, name, strlen(name) + 1);
        newFile.file.size = 0;
        newFile.file.data = nullptr;

        File* n = new File[folder->directory.numFiles + 1];
        memcpy(n, folder->directory.files, folder->directory.numFiles * sizeof(File));
        n[folder->directory.numFiles] = newFile;
        folder->directory.numFiles++;
        delete[] folder->directory.files;
        folder->directory.files = n;
        return true;
    }

    uint64 OpenFile(const char* path, uint64 mode) {
        File* file = FindFile(&g_RootFile, path + 1);
        if(file == nullptr || file->type != FILETYPE_FILE) {
            return 0;
        }

        if(file->status & File::StatusOpenedForWrite)
            return 0;
        if((mode & OpenFileModeWrite) && (file->status & File::StatusOpenedForRead))
            return 0;

        file->status |= mode;

        if(mode & OpenFileModeWrite) {
            delete[] file->file.data;
            file->file.size = 0;
            file->file.data = nullptr;
        }

        FileDescriptor desc;
        desc.file = file;
        desc.id = g_FileDescCounter;
        desc.mode = mode;
        desc.pos = 0;
        g_FileDescCounter++;

        g_FileDescriptors->push_back(desc);
        return desc.id;
    }

    void CloseFile(uint64 file) {
        for(auto a = g_FileDescriptors->begin(); a != g_FileDescriptors->end(); ++a) {
            if((*a).id == file) {
                (*a).file->status = 0;
                g_FileDescriptors->erase(a);
                return;
            }
        }
    }

    uint64 ReadFile(uint64 file, void* buffer, uint64 bufferSize) {
        for(auto d : *g_FileDescriptors) {
            if(d.id == file) {
                uint64 rem = d.file->file.size - d.pos;
                if(rem > bufferSize)
                    rem = bufferSize;

                memcpy(buffer, &d.file->file.data[d.pos], rem);
                d.pos += rem;
                return rem;
            }
        }

        return 0;
    }

    void WriteFile(uint64 file, void* buffer, uint64 bufferSize) {
        for(auto d : *g_FileDescriptors) {
            if(d.id == file) {
                uint64 newSize = d.file->file.size + bufferSize;
                char* newData = new char[newSize];
                memcpy(newData, d.file->file.data, d.file->file.size);
                delete[] d.file->file.data;
                d.file->file.data = newData;
                d.file->file.size = newSize;
                memcpy(&d.file->file.data[d.pos], buffer, bufferSize);
                d.pos += bufferSize;

                return;
            }
        }
    }

}