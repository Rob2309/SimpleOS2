#include "PipeFS.h"

#include "Mutex.h"
#include "ktl/vector.h"
#include "klib/memory.h"
#include "scheduler/Scheduler.h"
#include "klib/stdio.h"

namespace VFS {

    struct Pipe {
        uint64 id;
        bool free;
        Mutex lock;
        uint64 readPos;
        uint64 writePos;
        char buffer[4096];
    };

    static Mutex g_PipesLock;
    static ktl::vector<Pipe*> g_Pipes;

    void PipeFS::GetSuperBlock(SuperBlock* sb) { }

    void PipeFS::CreateNode(Node* node) {
        g_PipesLock.SpinLock();
        for(Pipe* p : g_Pipes) {
            if(p->free) {
                p->free = false;
                p->readPos = 0;
                p->writePos = 0;
                node->id = p->id;
                node->fs = this;
                node->linkRefCount = 0;
                g_PipesLock.Unlock();
                return;
            }
        }

        Pipe* newPipe = new Pipe();
        newPipe->id = g_Pipes.size();
        newPipe->free = false;
        newPipe->readPos = 0;
        newPipe->writePos = 0;
        g_Pipes.push_back(newPipe);
        g_PipesLock.Unlock();
        node->id = newPipe->id;
        node->fs = this;
        node->linkRefCount = 0;
    }
    void PipeFS::DestroyNode(Node* node) {
        g_PipesLock.SpinLock();
        g_Pipes[node->id]->free = true;
        g_PipesLock.Unlock();
    }

    void ReadNode(uint64 id, Node* node) { }
    void WriteNode(Node* node) { }

    static uint64 _Read(Pipe* p, void* buffer, uint64 bufferSize) {
        p->lock.SpinLock();

        if(p->readPos <= p->writePos) {
            uint64 rem = p->writePos - p->readPos;
            if(rem > bufferSize)
                rem = bufferSize;

            kmemcpy(buffer, p->buffer + p->readPos, rem);

            p->readPos += rem;
            p->lock.Unlock();
            return rem;
        } else {
            uint64 capA = sizeof(p->buffer) - p->readPos;
            uint64 capB = p->writePos;
            if(capA > bufferSize) {
                capA = bufferSize;
                capB = 0;
            } else if(capA + capB > bufferSize) {
                capB = bufferSize - capA;
            }

            kmemcpy(buffer, p->buffer + p->readPos, capA);
            kmemcpy(buffer + capA, p->buffer, capB);

            if(capB > 0) {
                p->readPos = capB;
            } else {
                p->readPos += capA;
                p->readPos %= sizeof(p->buffer);
            }

            p->lock.Unlock();
            return capA + capB;
        }
    }
    uint64 PipeFS::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize)  {
        g_PipesLock.SpinLock();
        Pipe* p = g_Pipes[node->id];
        g_PipesLock.Unlock();

        uint64 res;
        while((res = _Read(p, buffer, bufferSize)) == 0)
            Scheduler::ThreadYield();
        return res;
    }

    static uint64 _Write(Pipe* p, const void* buffer, uint64 bufferSize) {
        p->lock.SpinLock();

        if(p->writePos < p->readPos) {
            uint64 rem = p->readPos - p->writePos - 1;
            if(rem > bufferSize)
                rem = bufferSize;

            kmemcpy(p->buffer + p->writePos, buffer, rem);

            p->writePos += rem;
            p->lock.Unlock();
            return rem;
        } else if(p->readPos == 0) {
            uint64 rem = sizeof(p->buffer) - p->writePos;
            if(rem > bufferSize)
                rem = bufferSize;

            kmemcpy(p->buffer + p->writePos, buffer, rem);
            p->writePos += rem;
            p->writePos %= sizeof(p->buffer);

            p->lock.Unlock();
            return rem;
        } else {
            uint64 capA = sizeof(p->buffer) - p->writePos;
            uint64 capB = p->readPos - 1;
            if(capA > bufferSize) {
                capA = bufferSize;
                capB = 0;
            } else if(capA + capB > bufferSize) {
                capB = bufferSize - capA;
            }

            kmemcpy(p->buffer + p->writePos, buffer, capA);
            kmemcpy(p->buffer, buffer + capA, capB);

            if(capB > 0) {
                p->writePos = capB;
            } else {
                p->writePos += capA;
                p->writePos %= sizeof(p->buffer);
            }

            p->lock.Unlock();
            return capA + capB;
        }
    }
    uint64 PipeFS::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        g_PipesLock.SpinLock();
        Pipe* p = g_Pipes[node->id];
        g_PipesLock.Unlock();

        uint64 res = 0;
        while(true) {
            uint64 count = _Write(p, buffer + res, bufferSize - res);
            res += count;
            if(res == bufferSize)
                break;

            Scheduler::ThreadYield();
        }

        return bufferSize;
    }

    Directory* PipeFS::ReadDirEntries(Node* node) { }
    Directory* PipeFS::WriteDirEntries(Node* node) { }

}