#pragma once

#include "types.h"
#include "KernelHeader.h"

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
        Node* next;
        Node* prev;
        Segment seg;
    };

public:
    class Iterator
    {
    private:
        Node* m_Node;

    public:
        Iterator(Node* n) : m_Node(n) { }

        bool operator== (const Iterator& r) const { return m_Node == r.m_Node; }
        bool operator!= (const Iterator& r) const { return !(*this == r); }

        Iterator& operator++() { m_Node = m_Node->next; return *this; }
        Iterator& operator--() { m_Node = m_Node->prev; return *this; }

        Segment& operator*() { return m_Node->seg; }
        Segment* operator->() { return &m_Node->seg; }
    };

public:
    FreeList() : m_Head(nullptr), m_Tail(nullptr) { }
    FreeList(PhysicalMapSegment* list) : FreeList()
    {
        while(list != nullptr) {
            uint64 size = list->numPages * 4096;
            void* next = list->next;
            
            Node* n = (Node*)list;
            n->seg.base = (uint64)list;
            n->seg.size = size;

            AddNode(n);
            list = (PhysicalMapSegment*)next;
        }
    }

    Iterator begin() { return Iterator(m_Head); }
    Iterator end() { return Iterator(nullptr); }

    void* FindFree(uint64 size) 
    {
        Node* tmp = m_Head;
        while(tmp != nullptr) {
            if(tmp->seg.size >= size)
                return tmp;
            tmp = tmp->next;
        }
        return nullptr;
    }

    void MarkFree(void* base, uint64 size)
    {
        Node* node = (Node*)base;
        node->seg.base = (uint64)base;
        node->seg.size = size;

        AddNode(node);
        JoinAdjacent(node);
    }

    void MarkUsed(void* base, uint64 size) 
    {
        Node* node = (Node*)base;
        if(node->seg.size > size) {
            Node* newNode = (Node*)((char*)node + size);
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
    void RemoveNode(Node* node) 
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
    void AddNode(Node* node)
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
    void JoinAdjacent(Node* cmp) 
    {
        uint64 start = cmp->seg.base;
        uint64 end = start + cmp->seg.size;

        Node* tmp = m_Head;
        while(tmp != nullptr) {
            if(tmp->seg.base == end) {
                cmp->seg.size += tmp->seg.size;
                RemoveNode(tmp);
                JoinAdjacent(cmp);
                return;
            } else if(tmp->seg.base + tmp->seg.size == start) {
                tmp->seg.size += cmp->seg.size;
                RemoveNode(cmp);
                JoinAdjacent(cmp);
                return;
            }
            tmp = tmp->next;
        }
    }

private:
    Node* m_Head;
    Node* m_Tail;
};