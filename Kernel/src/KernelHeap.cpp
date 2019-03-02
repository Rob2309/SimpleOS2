#include "KernelHeap.h"

#include "MemoryManager.h"

#include "conio.h"

namespace KernelHeap {

    constexpr uint64 HeapBase = ((uint64)510 << 39) | 0xFFFF000000000000;

    struct Group {
        uint64 size;
        Group* next;
        Group* prev;
    };

    static Group* g_HeapStart;
    static Group* g_HeapEnd;
    static uint64 g_HeapPos;

    void Init()
    {
        void* g = MemoryManager::AllocatePages(1);
        MemoryManager::MapKernelPage(g, (void*)HeapBase);
        g_HeapStart = (Group*)HeapBase;
        g_HeapPos = (uint64)g_HeapStart + 4096;

        g_HeapStart->size = 4096;
        g_HeapStart->next = nullptr;
        g_HeapStart->prev = nullptr;

        g_HeapEnd = g_HeapStart;
    }

    static Group* FindGroup(uint64 size)
    {
        Group* temp = g_HeapStart;

        while(temp != nullptr) {
            if(temp->size >= size)
                return temp;

            temp = temp->next;
        }

        return nullptr;
    }

    static void AddGroupToList(Group* g)
    {
        if(g_HeapStart == nullptr) {
            g_HeapStart = g;
            g_HeapEnd = g;
        } else {
            g_HeapEnd->next = g;
            g->prev = g_HeapEnd;
            g_HeapEnd = g;
        }
    }

    static void RemoveGroupFromList(Group* g)
    {
        if(g->prev != nullptr)
            g->prev->next = g->next;
        else
            g_HeapStart = g->next;

        if(g->next != nullptr)
            g->next->prev = g->prev;
        else
            g_HeapEnd = g->prev;
    }

    static void JoinAdjacent(Group* cmp)
    {
        uint64 start = (uint64)cmp;
        uint64 end = start + cmp->size;

        Group* group = g_HeapStart;
        while(group != nullptr) {
            if((uint64)group == end) {
                cmp->size += group->size;
                RemoveGroupFromList(group);
                JoinAdjacent(cmp);
                return;
            } else if((uint64)group + group->size == start) {
                group->size += cmp->size;
                RemoveGroupFromList(cmp);
                JoinAdjacent(group);
                return;
            }
            group = group->next;
        }
    }

    static Group* CreateGroup(uint64 size) 
    {
        size = (size + 4095) / 4096;
        void* g = MemoryManager::AllocatePages(size);
        for(int i = 0; i < size; i++)
            MemoryManager::MapKernelPage((char*)g + 4096 * i, (char*)g_HeapPos + 4096 * i);
        Group* newGroup = (Group*)g_HeapPos;
        newGroup->size = size * 4096;
        newGroup->next = nullptr;
        newGroup->prev = nullptr;
        AddGroupToList(newGroup);
        JoinAdjacent(newGroup);
        g_HeapPos += size * 4096;
        return newGroup;
    }

    void* Allocate(uint64 size)
    {
        size = (size + sizeof(uint64) + 63) / 64 * 64;

        Group* g = FindGroup(size);
        if(g == nullptr)
            g = CreateGroup(size);
        
        uint64* res = (uint64*)g;

        g->size -= size;
        if(g->size == 0) {
            RemoveGroupFromList(g);
        } else {
            uint64 s = g->size;

            RemoveGroupFromList(g);

            g = (Group*)((char*)g + size);
            g->size = s;
            g->next = nullptr;
            g->prev = nullptr;

            AddGroupToList(g);
        }

        *res = size;
        res++;
        return res;
    }
    void Free(void* block)
    {
        uint64 size = *((uint64*)block - 1);
        Group* g = (Group*)((uint64*)block - 1);
        g->size = size;
        g->next = nullptr;
        g->prev = nullptr;
        AddGroupToList(g);
        JoinAdjacent(g);
    }

}