#pragma once

#include "types.h"
#include "KernelHeader.h"

#include "AnchorList.h"

namespace ktl {

    struct FreeListSegment
    {
        Anchor<FreeListSegment> anchor;
        uint64 base;
        uint64 size;
    };

    /**
     * This list keeps track of free spaces in memory. It stores its entries directly in the free spaces instead of allocating extra memory for them.
     * It is used in the MemoryManager
     **/
    class FreeList : public AnchorList<FreeListSegment, &FreeListSegment::anchor>
    {
    public:
        void Init(PhysicalMapSegment* list) {
            while(list != nullptr) {
                uint64 size = list->numPages * 4096;
                void* next = list->next;
                
                auto n = reinterpret_cast<FreeListSegment*>(list);
                n->base = (uint64)list;
                n->size = size;

                push_back(n);
                list = (PhysicalMapSegment*)next;
            }
        }

        /**
         * Find the first free segment that has enough capacity to fit the number of bytes requested
         **/
        void* FindFree(uint64 size) 
        {
            for(auto& seg : *this) {
                if(seg.size >= size)
                    return &seg;
            }
        }

        /**
         * Marks the given bytes as available
         **/
        void MarkFree(void* base, uint64 size)
        {
            auto node = static_cast<FreeListSegment*>(base);
            node->base = (uint64)base;
            node->size = size;

            push_back(node);
            JoinAdjacent(node);
        }

        /**
         * Marks the given bytes as occupied
         **/
        void MarkUsed(void* base, uint64 size) 
        {
            auto node = static_cast<FreeListSegment*>(base);
            erase(node);

            if(node->size > size) {
                auto newNode = reinterpret_cast<FreeListSegment*>((char*)node + size);
                newNode->base = node->base + size;
                newNode->size = node->size - size;

                push_back(newNode);
                JoinAdjacent(newNode);
            }
        }

    private:
        void JoinAdjacent(FreeListSegment* cmp) 
        {
            uint64 start = cmp->base;
            uint64 end = start + cmp->size;

            for(auto& tmp : *this) {
                if(tmp.base == end) {
                    cmp->size += tmp.size;
                    erase(&tmp);
                    return JoinAdjacent(cmp);
                } else if(tmp.base + tmp.size == start) {
                    tmp.size += cmp->size;
                    erase(cmp);
                    return JoinAdjacent(&tmp);
                }
            }
        }
    };

}