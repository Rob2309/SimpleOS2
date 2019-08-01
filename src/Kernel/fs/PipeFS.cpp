#include "PipeFS.h"

#include "ktl/vector.h"
#include "klib/memory.h"
#include "scheduler/Scheduler.h"
#include "klib/stdio.h"

namespace VFS {

    void PipeFS::GetSuperBlock(SuperBlock* sb, void* infoPtr) { }

    void PipeFS::CreateNode(Node* node) {
        m_PipesLock.Spinlock();
        for(Pipe* p : m_Pipes) {
            if(p->free) {
                p->free = false;
                p->readPos = 0;
                p->writePos = 0;
                node->id = p->id;
                node->fs = this;
                node->linkRefCount = 0;
                m_PipesLock.Unlock();
                return;
            }
        }

        Pipe* newPipe = new Pipe();
        newPipe->id = m_Pipes.size();
        newPipe->free = false;
        newPipe->readPos = 0;
        newPipe->writePos = 0;
        m_Pipes.push_back(newPipe);
        m_PipesLock.Unlock();
        node->id = newPipe->id;
        node->fs = this;
        node->linkRefCount = 0;
    }
    void PipeFS::DestroyNode(Node* node) {
        m_PipesLock.Spinlock();
        m_Pipes[node->id]->free = true;
        m_PipesLock.Unlock();
    }

    void ReadNode(uint64 id, Node* node) { }
    void WriteNode(Node* node) { }

    static uint64 _Read(Pipe* p, void* buffer, uint64 bufferSize) {
        char* realBuffer = (char*)buffer;

        p->lock.Spinlock();

        if(p->readPos == p->writePos) {
            p->lock.Unlock();
            return 0;
        } else if(p->readPos < p->writePos) {
            uint64 rem = p->writePos - p->readPos;
            if(rem > bufferSize)
                rem = bufferSize;

            if(!kmemcpy_usersafe(realBuffer, p->buffer + p->readPos, rem)) {
                p->lock.Unlock();
                return ErrorInvalidBuffer;
            }

            p->readPos += rem;
            p->lock.Unlock();
            return rem;
        } else {
            uint64 remA = sizeof(p->buffer) - p->readPos;
            uint64 remB = p->writePos;

            if(remA > bufferSize) {
                remA = bufferSize;
                remB = 0;
            } else if(remA + remB > bufferSize)
                remB = bufferSize - remA;

            if(!kmemcpy_usersafe(realBuffer, p->buffer + p->readPos, remA)) {
                p->lock.Unlock();
                return ErrorInvalidBuffer;
            }
            if(!kmemcpy_usersafe(realBuffer + remA, p->buffer, remB)) {
                p->lock.Unlock();
                return ErrorInvalidBuffer;
            }

            p->readPos += remA + remB;
            p->readPos %= sizeof(p->buffer);
            p->lock.Unlock();
            return remA + remB;
        }
    }
    uint64 PipeFS::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize)  {
        m_PipesLock.Spinlock();
        Pipe* p = m_Pipes[node->id];
        m_PipesLock.Unlock();

        uint64 res;
        while((res = _Read(p, buffer, bufferSize)) == 0)
            Scheduler::ThreadYield();
        return res;
    }

    static uint64 _Write(Pipe* p, const void* buffer, uint64 bufferSize) {
        char* realBuffer = (char*)buffer;

        p->lock.Spinlock();

        if(p->writePos < p->readPos) {
            uint64 rem = p->readPos - p->writePos - 1;
            if(rem > bufferSize)
                rem = bufferSize;

            if(!kmemcpy_usersafe(p->buffer + p->writePos, realBuffer, rem)) {
                p->lock.Unlock();
                return ErrorInvalidBuffer;
            }

            p->writePos += rem;
            p->lock.Unlock();
            return rem;
        } else {
            if(p->readPos > 0) {
                uint64 remA = sizeof(p->buffer) - p->writePos;
                uint64 remB = p->readPos - 1;
                if(remA > bufferSize) {
                    remA = bufferSize;
                    remB = 0;
                } else if(remA + remB > bufferSize)
                    remB = bufferSize - remA;

                if(!kmemcpy_usersafe(p->buffer + p->writePos, realBuffer, remA)) {
                    p->lock.Unlock();
                    return ErrorInvalidBuffer;
                }
                if(!kmemcpy_usersafe(p->buffer, realBuffer + remA, remB)) {
                    p->lock.Unlock();
                    return ErrorInvalidBuffer;
                }

                p->writePos += remA + remB;
                p->writePos %= sizeof(p->buffer);
                p->lock.Unlock();
                return remA + remB;
            } else {
                uint64 rem = sizeof(p->buffer) - p->writePos - 1;
                if(rem > bufferSize)
                    rem = bufferSize;

                if(!kmemcpy_usersafe(p->buffer + p->writePos, realBuffer, rem)) {
                    p->lock.Unlock();
                    return ErrorInvalidBuffer;
                }

                p->writePos += rem;
                p->lock.Unlock();
                return rem;
            }
        }
    }
    uint64 PipeFS::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        char* realBuffer = (char*)buffer;

        m_PipesLock.Spinlock();
        Pipe* p = m_Pipes[node->id];
        m_PipesLock.Unlock();

        uint64 res = 0;
        while(true) {
            uint64 count = _Write(p, realBuffer + res, bufferSize - res);
            if(count == ErrorInvalidBuffer)
                return ErrorInvalidBuffer;
            res += count;
            if(res == bufferSize)
                break;

            Scheduler::ThreadYield();
        }

        return bufferSize;
    }
    void PipeFS::ClearNodeData(Node* node) { }

    Directory* PipeFS::ReadDirEntries(Node* node) { return nullptr; }
    void PipeFS::WriteDirEntries(Node* node) { }

}