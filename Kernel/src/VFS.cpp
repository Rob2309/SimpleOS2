#include "VFS.h"

#include "ArrayList.h"
#include "string.h"
#include "conio.h"
#include "Device.h"

namespace VFS {

    struct File
    {
        static constexpr uint64 StatusOpenedForRead = 0x1;
        static constexpr uint64 StatusOpenedForWrite = 0x2;

        enum Type {
            TYPE_FILE,
            TYPE_DIRECTORY,
            TYPE_DEVICE,
        } type;

        union {
            struct {
                uint64 size;
                char* data;
            } file;
            struct {
                uint64 numFiles;
                File* files;
            } directory;
            struct {
                uint64 devID;
            } device;
        };

        uint64 numReaders;
        uint64 numWriters;

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
        g_RootFile.type = File::TYPE_DIRECTORY;
        g_RootFile.directory.numFiles = 0;
        g_RootFile.directory.files = nullptr;
        g_RootFile.numReaders = 0;
        g_RootFile.numWriters = 0;
        memset(g_RootFile.name, 0, sizeof(g_RootFile.name));

        g_FileDescriptors = new ArrayList<FileDescriptor>();
    }

    static File* FindFile(File* folder, const char* path)
    {
        if(path[0] == '\0') {
            return folder;
        }
        if(folder->type != File::TYPE_DIRECTORY) {
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

    static File* CreateNode(const char* path, const char* name, File::Type type)
    {
        File* folder = FindFile(&g_RootFile, path + 1);
        if(folder == nullptr)
            return nullptr;
        if(folder->type != File::TYPE_DIRECTORY)
            return nullptr;

        File newFile;
        newFile.type = type;
        newFile.numReaders = 0;
        newFile.numWriters = 0;
        memcpy(newFile.name, name, strlen(name) + 1);
        newFile.file.size = 0;
        newFile.file.data = nullptr;

        File* n = new File[folder->directory.numFiles + 1];
        memcpy(n, folder->directory.files, folder->directory.numFiles * sizeof(File));
        n[folder->directory.numFiles] = newFile;
        folder->directory.numFiles++;
        delete[] folder->directory.files;
        folder->directory.files = n;
        return &folder->directory.files[folder->directory.numFiles - 1];
    }

    bool CreateFile(const char* folder, const char* name) {
        File* f = CreateNode(folder, name, File::TYPE_FILE);
        if(f == nullptr)
            return false;

        f->file.size = 0;
        f->file.data = nullptr;
        return true;
    }

    bool CreateFolder(const char* folder, const char* name) {
        File* f = CreateNode(folder, name, File::TYPE_DIRECTORY);
        if(f == nullptr)
            return false;

        f->directory.numFiles = 0;
        f->directory.files = nullptr;
        return true;
    }

    bool CreateDeviceFile(const char* folder, const char* name, uint64 devID) {
        File* f = CreateNode(folder, name, File::TYPE_DEVICE);
        if(f == nullptr)
            return false;

        f->device.devID = devID;
        return true;
    }

    bool RemoveFile(const char* file) {
        return false;
    }

    uint64 OpenFile(const char* path, uint64 mode) {
        File* file = FindFile(&g_RootFile, path + 1);
        if(file == nullptr || file->type == File::TYPE_DIRECTORY) {
            return 0;
        }

        if(file->numWriters > 0)
            return 0;
        if((mode & OpenFileModeWrite) && file->numReaders > 0)
            return 0;

        file->numReaders += (mode & OpenFileModeRead ? 1 : 0);
        file->numWriters += (mode & OpenFileModeWrite ? 1 : 0);

        FileDescriptor desc;
        desc.file = file;
        desc.id = g_FileDescCounter;
        desc.mode = mode;
        desc.pos = 0;
        g_FileDescCounter++;

        if(file->type == File::TYPE_DEVICE)
            Device::GetByID(file->device.devID)->SetPos(0);

        g_FileDescriptors->push_back(desc);
        return desc.id;
    }

    void CloseFile(uint64 file) {
        for(auto a = g_FileDescriptors->begin(); a != g_FileDescriptors->end(); ++a) {
            if(a->id == file) {
                a->file->numReaders -= (a->mode & OpenFileModeRead ? 1 : 0);
                a->file->numWriters -= (a->mode & OpenFileModeWrite ? 1 : 0);
                g_FileDescriptors->erase(a);
                return;
            }
        }
    }

    void SeekFile(uint64 file, uint64 pos)
    {
        for(auto d : *g_FileDescriptors) {
            if(d.id == file) {
                if(d.file->type == File::TYPE_FILE) {
                    if(d.file->file.size >= pos)
                        d.pos = d.file->file.size - 1;
                    else
                        d.pos = pos;
                } else if(d.file->type == File::TYPE_DEVICE) {
                    Device::GetByID(d.file->device.devID)->SetPos(pos);
                }
                return;
            }
        }
    }

    uint64 ReadFile(uint64 file, void* buffer, uint64 bufferSize) {
        for(auto d : *g_FileDescriptors) {
            if(d.id == file) {
                if(d.file->type == File::TYPE_FILE) {
                    uint64 rem = d.file->file.size - d.pos;
                    if(rem > bufferSize)
                        rem = bufferSize;

                    memcpy(buffer, &d.file->file.data[d.pos], rem);
                    d.pos += rem;
                    return rem;
                } else if(d.file->type == File::TYPE_DEVICE) {
                    return Device::GetByID(d.file->device.devID)->Read(buffer, bufferSize);
                }
            }
        }

        return 0;
    }

    void WriteFile(uint64 file, void* buffer, uint64 bufferSize) {
        for(auto d : *g_FileDescriptors) {
            if(d.id == file) {
                if(d.file->type == File::TYPE_FILE) {
                    uint64 newSize = d.file->file.size + bufferSize;
                    char* newData = new char[newSize];
                    memcpy(newData, d.file->file.data, d.file->file.size);
                    delete[] d.file->file.data;
                    d.file->file.data = newData;
                    d.file->file.size = newSize;
                    memcpy(&d.file->file.data[d.pos], buffer, bufferSize);
                    d.pos += bufferSize;
                } else if(d.file->type == File::TYPE_DEVICE) {
                    Device::GetByID(d.file->device.devID)->Write(buffer, bufferSize);
                }

                return;
            }
        }
    }

}