#include "stdlib.h"

#include "Syscall.h"

class FreeList
{
public:
    struct Segment
    {
        uint64 base;
        uint64 size;
    };
private:
    struct Node
    {
        volatile Node* next;
        volatile Node* prev;
        Segment seg;
    };

public:
    class Iterator
    {
    private:
        volatile Node* m_Node;

    public:
        Iterator(volatile Node* n) : m_Node(n) { }

        bool operator== (const Iterator& r) const { return m_Node == r.m_Node; }
        bool operator!= (const Iterator& r) const { return !(*this == r); }

        Iterator& operator++() { m_Node = m_Node->next; return *this; }
        Iterator& operator--() { m_Node = m_Node->prev; return *this; }

        volatile Segment& operator*() { return m_Node->seg; }
        volatile Segment* operator->() { return &m_Node->seg; }
    };

public:
    FreeList() : m_Head(nullptr), m_Tail(nullptr) { }

    Iterator begin() { return Iterator(m_Head); }
    Iterator end() { return Iterator(nullptr); }

    /**
     * Find the first free segment that has enough capacity to fit the number of bytes requested
     **/
    void* FindFree(uint64 size) 
    {
        volatile Node* tmp = m_Head;
        while(tmp != nullptr) {
            if(tmp->seg.size >= size)
                return (void*)tmp;
            tmp = tmp->next;
        }
        return nullptr;
    }

    /**
     * Marks the given bytes as available
     **/
    void MarkFree(void* base, uint64 size)
    {
        volatile Node* node = (volatile Node*)base;
        node->seg.base = (uint64)base;
        node->seg.size = size;

        AddNode(node);
        JoinAdjacent(node);
    }

    /**
     * Marks the given bytes as occupied
     **/
    void MarkUsed(void* base, uint64 size) 
    {
        volatile Node* node = (volatile Node*)base;
        if(node->seg.size > size) {
            volatile Node* newNode = (volatile Node*)((char*)node + size);
            newNode->next = node->next;
            newNode->prev = node->prev;
            newNode->seg.base = node->seg.base + size;
            newNode->seg.size = node->seg.size - size;

            if(node->prev != nullptr)
                node->prev->next = newNode;
            else
                m_Head = newNode;
            if(node->next != nullptr)
                node->next->prev = newNode;
            else
                m_Tail = newNode;
        } else {
            RemoveNode(node);
        }
    }

private:
    void RemoveNode(volatile Node* node) 
    {
        if(node->prev != nullptr)
            node->prev->next = node->next;
        else
            m_Head = node->next;
        
        if(node->next != nullptr)
            node->next->prev = node->prev;
        else
            m_Tail = node->prev;
    }
    void AddNode(volatile Node* node)
    {
        if(m_Head == nullptr) {
            m_Head = m_Tail = node;
            node->prev = nullptr;
            node->next = nullptr;
        } else {
            m_Tail->next = node;
            node->prev = m_Tail;
            node->next = nullptr;
            m_Tail = node;
        }
    }
    void JoinAdjacent(volatile Node* cmp) 
    {
        uint64 start = cmp->seg.base;
        uint64 end = start + cmp->seg.size;

        volatile Node* tmp = m_Head;
        while(tmp != nullptr) {
            if(tmp->seg.base == end) {
                cmp->seg.size += tmp->seg.size;
                RemoveNode(tmp);
                JoinAdjacent(cmp);
                return;
            } else if(tmp->seg.base + tmp->seg.size == start) {
                tmp->seg.size += cmp->seg.size;
                RemoveNode(cmp);
                JoinAdjacent(tmp);
                return;
            }
            tmp = tmp->next;
        }
    }

private:
    volatile Node* m_Head;
    volatile Node* m_Tail;
};

static FreeList g_FreeList;
static uint64 g_HeapPos = 0xFF000000;

static void ReserveNew(uint64 size) 
{
    size = (size + 4095) / 4096;
    Syscall::AllocPages((void*)g_HeapPos, size);
    g_FreeList.MarkFree((void*)g_HeapPos, size * 4096);
    g_HeapPos += size * 4096;
}

void* malloc(int64 size)
{
    size = (size + sizeof(uint64) * 2 + 63) / 64 * 64;

    void* g = g_FreeList.FindFree(size);
    if(g == nullptr) {
        ReserveNew(size);
        g = g_FreeList.FindFree(size);
    }
    g_FreeList.MarkUsed(g, size);

    *(uint64*)g = size;
    return (uint64*)g + 2;
}
void free(void* block)
{
    if(block == nullptr)
        return;

    uint64* b = (uint64*)block - 2;
    uint64 size = *b;

    g_FreeList.MarkFree(b, size);
}

void abort() {
    Syscall::Exit(1);
}