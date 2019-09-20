#pragma once

#include "types.h"
#include "klib/memory.h"
#include "new.h"

namespace ktl {

    template<typename T>
    class vector
    {
    public:
        class Iterator
        {
        public:
            Iterator(T* data, uint64 pos) : m_Data(data), m_Pos(pos) { }

            Iterator& operator++() { m_Pos++; return *this; }
            Iterator& operator--() { m_Pos--; return *this; }

            T& operator* () { return m_Data[m_Pos]; }
            T* operator-> () { return &m_Data[m_Pos]; }

            bool operator== (const Iterator& r) const { return m_Pos == r.m_Pos; }
            bool operator!= (const Iterator& r) const { return !(*this == r); }

        public:
            uint64 m_Pos;
            T* m_Data;
        };

    public:
        vector()
            : m_Capacity(0), m_Size(0), m_Data(nullptr)
        { }
        ~vector()
        {
            if(m_Capacity != 0)
                delete[] m_Data;
        }

        vector(const vector& r)
        {
            m_Capacity = 0;
            m_Size = 0;
            MakeCapacity(r.m_Size);

            m_Size = r.m_Size;
            for(uint64 i = 0; i < m_Size; i++)
                new(&m_Data[i]) T(r.m_Data[i]);
        }
        vector(vector&& r)
            : m_Capacity(r.m_Capacity), m_Size(r.m_Size), m_Data(r.m_Data)
        {
            r.m_Capacity = 0;
            r.m_Data = nullptr;
        }

        void push_back(const T& t)
        {
            if(m_Capacity < m_Size + 1) {
                MakeCapacity(m_Size + 1);
            }

            new(&m_Data[m_Size]) T(t);
            m_Size++;
        }
        void pop_back() {
            erase(--end());
        }

        T& back() {
            return m_Data[m_Size - 1];
        }
        const T& back() const {
            return m_Data[m_Size - 1];
        }
        
        Iterator erase(const Iterator& it)
        {
            m_Data[it.m_Pos].~T();
            for(uint64 i = it.m_Pos + 1; i < m_Size; i++) {
                new(&m_Data[i - 1]) T((T&&)m_Data[i]);
                m_Data[i].~T();
            }
            m_Size--;

            return Iterator(m_Data, it.m_Pos);
        }

        Iterator insert(const Iterator& at, const T& t) {
            if(m_Capacity < m_Size + 1) {
                MakeCapacity(m_Size + 1);
            }

            for(int i = m_Size; i > at.m_Pos; i--) {
                new(&m_Data[i]) T((T&&)m_Data[i-1]);
                m_Data[i-1].~T();
            }

            new(&m_Data[at.m_Pos]) T(t);
            m_Size++;

            return Iterator(m_Data, at.m_Pos);
        }

        T& operator[] (uint64 index) {
            return m_Data[index];
        }
        const T& operator[] (uint64 index) const {
            return m_Data[index];
        }

        uint64 size() const { return m_Size; }

        Iterator begin() { return Iterator(m_Data, 0); }
        Iterator end() { return Iterator(m_Data, m_Size); }

    private:
        void MakeCapacity(uint64 size)
        {
            m_Capacity = size;
            T* n = (T*)new char[size * sizeof(T)];

            if(m_Size != 0)
            {
                for(uint64 i = 0; i < m_Size; i++)
                {
                    new(&n[i]) T((T&&)m_Data[i]);
                    m_Data[i].~T();
                }
                delete[] (char*)m_Data;
            }

            m_Data = n;
        }

    private:
        uint64 m_Capacity;
        uint64 m_Size;
        T* m_Data;
    };
    
}