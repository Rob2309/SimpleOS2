#pragma once

namespace std {

    template<typename T>
    class list
    {
    public:
        struct Node {
            Node* next;
            Node* prev;
            T obj;
        };
        class Iterator
        {
        public:
            explicit Iterator(Node* node) : m_Node(node) { }

            Iterator& operator++() { m_Node = m_Node->next; return *this; }
            Iterator& operator--() { m_Node = m_Node->prev; return *this; }

            T& operator* () { return m_Node->obj; }
            T* operator-> () { return &m_Node->obj; }

            bool operator== (const Iterator& r) const { return m_Node == r.m_Node; }
            bool operator!= (const Iterator& r) const { return !(*this == r); }
        public:
            Node* m_Node;
        };

    public:
        list()
            : m_Head(nullptr), m_Tail(nullptr)
        { }
        ~list()
        {
            Node* n = m_Head;
            while(n != nullptr) {
                Node* tmp = n->next;
                delete n;
                n = tmp;
            }
        }

        list(const list& t) = delete;
        list(const list&& t) = delete;

        T& back() { return m_Tail->obj; }

        void push_back(const T& t)
        {
            Node* n = new Node();
            n->obj = t;
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
        void pop_back()
        {
            erase(--end());
        }

        void pop_front()
        {
            erase(begin());
        }

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
            
            delete n;
        }
        void erase(const T& obj)
        {
            for(auto a = begin(); a != end(); ++a)
                if(*a == obj) {
                    erase(a);
                    return;
                }
        }

        Iterator begin() { return Iterator(m_Head); }
        Iterator end() { return Iterator(nullptr); }

        bool empty() const { return m_Head == nullptr; }

    private:
        Node* m_Head;
        Node* m_Tail;
    };

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

        Node& back() { return *m_Tail; }

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
        void pop_back()
        {
            erase(--end());
        }

        void pop_front()
        {
            erase(begin());
        }

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

        bool empty() const { return m_Head == nullptr; }

    private:
        Node* m_Head;
        Node* m_Tail;
    };

}