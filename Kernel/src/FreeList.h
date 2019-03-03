#pragma once

#include "types.h"

class FreeList
{
private:
    struct Node
    {
        Node* next;
        Node* prev;
        uint64 base;
        uint64 size;
    };

public:
    FreeList() : m_Head(nullptr), m_Tail(nullptr) { }

    void* FindFree(uint64 size) 
    {
        Node* tmp = m_Head;
        while(tmp != nullptr) {
            if(tmp->size >= size)
                return tmp;
            tmp = tmp->next;
        }
        return nullptr;
    }

    void MarkFree(void* base, uint64 size)
    {
        Node* node = (Node*)base;
        node->base = (uint64)base;
        node->size = size;

        AddNode(node);
        JoinAdjacent(node);
    }

    void MarkUsed(void* base, uint64 size) 
    {
        Node* node = (Node*)base;
        if(node->size > size) {
            Node* newNode = (Node*)((char*)node + size);
            newNode->next = node->next;
            newNode->prev = node->prev;
            newNode->base = node->base + size;
            newNode->size = node->size - size;

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
        uint64 start = cmp->base;
        uint64 end = start + cmp->size;

        Node* tmp = m_Head;
        while(tmp != nullptr) {
            if(tmp->base == end) {
                cmp->size += tmp->size;
                RemoveNode(tmp);
                JoinAdjacent(cmp);
                return;
            } else if(tmp->base + tmp->size == start) {
                tmp->size += cmp->size;
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