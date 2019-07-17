#pragma once

/**
 * This is a special list class that does not allocate or free any memory.
 * Instead, the type that is stored in the list has to have a 'prev' and 'next' member.
 **/
template<typename Node>
class nlist
{
public:
    class Iterator
    {
    public:
        explicit Iterator(Node* node) : m_Node(node) { }

        Iterator& operator++() { m_Node = m_Node->next; return *this; }
        Iterator& operator--() { m_Node = m_Node->prev; return *this; }

        Iterator operator++(int) { 
            Node* res = m_Node;
            m_Node = m_Node->next;
            return Iterator(res);
        }
        Iterator operator--(int) {
            Node* res = m_Node;
            m_Node = m_Node->prev;
            return Iterator(res);
        }

        Node* operator* () { return m_Node; }
        Node* operator-> () { return m_Node; }

        bool operator== (const Iterator& r) const { return m_Node == r.m_Node; }
        bool operator!= (const Iterator& r) const { return !(*this == r); }
    public:
        Node* m_Node;
    };

public:
    nlist()
        : m_Head(nullptr), m_Tail(nullptr)
    { }

    nlist(const nlist& t) = delete;
    nlist(const nlist&& t) = delete;

    Node* front() { return m_Head; }
    Node* back() { return m_Tail; }

    /**
     * Add a node to the end of the list.
     **/
    void push_back(Node* n)
    {
        n->next = nullptr;
        n->prev = m_Tail;
        if(m_Head == nullptr) {
            m_Head = m_Tail = n;
        } else {
            m_Tail->next = n;
            n->prev = m_Tail;
            m_Tail = n;
        }
    }
    /**
     * Remove a node from the end of the list (note: the element will not be freed)
     **/
    void pop_back()
    {
        erase(before_end());
    }

    /**
     * Remove a node from the front of the list (note: the element will not be freed)
     **/
    void pop_front()
    {
        erase(begin());
    }

    /**
     * Remove a node from the list at the position given by the iterator (note: the element will not be freed)
     **/
    void erase(const Iterator& it)
    {
        Node* n = it.m_Node;
        
        if(n->prev != nullptr) {
            n->prev->next = n->next;
        } else {
            m_Head = n->next;
        }
        if(n->next != nullptr) {
            n->next->prev = n->prev;
        } else {
            m_Tail = n->prev;
        }
    }
    /**
     * Remove the first (if any) occurence of the given element from the list (note: the element will not be freed)
     **/
    void erase(const Node* node)
    {
        for(auto a = begin(); a != end(); ++a)
            if(a.m_Node == node) {
                erase(a);
                return;
            }
    }

    Iterator begin() { return Iterator(m_Head); }
    Iterator end() { return Iterator(nullptr); }
    Iterator before_end() { return Iterator(m_Tail); }

    bool empty() const { return m_Head == nullptr; }

private:
    Node* m_Head;
    Node* m_Tail;
};