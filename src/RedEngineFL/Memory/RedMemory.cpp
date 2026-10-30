#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "RedMemory.h"
#include "RedEngineFL/Memory/engineAlloc.h"

extern "C"
{
    bool isHeapObjInitialized;
    HeapObj* heapobj;
    int maxheaps;
    int __RedCurrHeap;
    int __RedDebugHeap = -1;
    int __RedTempHeap = -1;
    void (*OnFailure)(int heap, unsigned int size);
    int sRecursionDepth;
}

// OpenSWBF2 -heapcheck: every block keeps the code addresses that allocated it (as image RVAs) in
// the header bytes its Tag leaves spare, and an allocation that meets a free-list entry outside
// its heap stops and reports the heap. Blocks sit end to end, so the last intact block before the
// first broken Tag in address order is the one that ran over it.
extern "C" __declspec(dllimport) unsigned short __stdcall RtlCaptureStackBackTrace(unsigned long, unsigned long, void**, unsigned long*);
extern "C" __declspec(dllimport) void* __stdcall GetModuleHandleA(const char*);
extern "C" __declspec(dllimport) char* __stdcall GetCommandLineA();
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);
extern "C" bool gRedHeapCheck; // RedMemoryPool.cpp

static unsigned int HeapCheckSlots(unsigned int hdr)
{
    const unsigned int uiSpare = hdr > sizeof(Tag) ? hdr - (unsigned int)sizeof(Tag) : 0;
    return uiSpare / 4 < 2 ? uiSpare / 4 : 2;
}

static void HeapCheckRecord(Tag* tag, unsigned int hdr)
{
    void* apFrames[4] = {0, 0, 0, 0};
    RtlCaptureStackBackTrace(0, 4, apFrames, 0);
    const char* pBase = (const char*)GetModuleHandleA(0);
    unsigned int* puSlots = (unsigned int*)((char*)tag + sizeof(Tag));
    const unsigned int uiSlots = HeapCheckSlots(hdr);
    for (unsigned int i = 0; i < uiSlots; ++i)
    {
        // frames 0-1 are the allocator itself; 2-3 are its caller and theirs
        puSlots[i] = (unsigned int)((const char*)apFrames[2 + i] - pBase);
    }
}

static bool HeapCheckTagOk(const HeapObj& h, const Tag* tag)
{
    const char* p = (const char*)tag;
    return p >= (const char*)h.start && p < (const char*)h.start + h.size &&
           tag->size != 0 && tag->size <= h.size;
}

static void HeapCheckReport(int heap, const Tag* pBad)
{
    const HeapObj& h = heapobj[heap];
    const unsigned int hdr = (h.align + sizeof(Tag) - 1) & ~(h.align - 1);
    const unsigned int uiSlots = HeapCheckSlots(hdr);
    char acLine[256];
    _snprintf(acLine, sizeof(acLine), "OPENSWBF2 HEAPCHECK heap %d (%s): bad free-list entry %p\n", heap,
              h.name ? h.name : "?", pBad);
    OutputDebugStringA(acLine);
    fputs(acLine, stderr);

    const char* p = (const char*)h.start;
    const char* const pEnd = p + h.size;
    const Tag* apLast[3] = {0, 0, 0};
    while (p < pEnd)
    {
        const Tag* tag = (const Tag*)p;
        if (tag->size == 0 || tag->size > (unsigned int)(pEnd - p) || (tag->size & (h.align - 1)) != 0)
        {
            break;
        }
        apLast[2] = apLast[1];
        apLast[1] = apLast[0];
        apLast[0] = tag;
        p += tag->size;
    }
    _snprintf(acLine, sizeof(acLine), "OPENSWBF2 HEAPCHECK first broken tag at %p (size %u); blocks before it:\n",
              p, p < pEnd ? ((const Tag*)p)->size : 0);
    OutputDebugStringA(acLine);
    fputs(acLine, stderr);
    for (int i = 0; i < 3 && apLast[i]; ++i)
    {
        const unsigned int* puSlots = (const unsigned int*)((const char*)apLast[i] + sizeof(Tag));
        _snprintf(acLine, sizeof(acLine), "OPENSWBF2 HEAPCHECK   %p size %u user %u allocated from rva 0x%X / 0x%X\n",
                  apLast[i], apLast[i]->size, apLast[i]->size - hdr, uiSlots > 0 ? puSlots[0] : 0,
                  uiSlots > 1 ? puSlots[1] : 0);
        OutputDebugStringA(acLine);
        fputs(acLine, stderr);
    }
}

// FUNCTION: SWBF2 0x006c3a50
extern "C" unsigned int RedGetHeapFree(int heap)
{
    unsigned int total = 0;
    for (Tag* tag = heapobj[heap].free.first; tag; tag = tag->next_)
    {
        total += tag->size;
    }
    return total;
}

extern "C" char const* RedGetHeapName(int heap)
{
    if (heap >= 0 && heap < maxheaps && heapobj[heap].name)
    {
        return heapobj[heap].name;
    }
    return "?";
}

namespace RedMemory
{
    int failHeap;
    unsigned int failSize;
}

extern "C" void Startup_RedInitHeap();

void TagAddList(TagList* list, Tag* tag);
void TagMergeIntoList(TagList* list, Tag* tag);
void TagRemoveList(TagList* list, Tag* tag);

// FUNCTION: SWBF2 0x006c3540
void* __cdecl operator new(size_t size)
{
    return RedAllocFromHeap(__RedCurrHeap, size, 0);
}

// FUNCTION: SWBF2 0x006c3560
void __cdecl operator delete(void* block)
{
    if (block)
    {
        RedFreeToHeap(__RedCurrHeap, block, 0);
    }
}

// FUNCTION: SWBF2 0x006c3580
void* __cdecl operator new[](size_t size)
{
    return RedAllocFromHeap(__RedCurrHeap, size, 0);
}

// FUNCTION: SWBF2 0x006c35a0
void __cdecl operator delete[](void* block)
{
    if (block)
    {
        RedFreeToHeap(__RedCurrHeap, block, 0);
    }
}

// FUNCTION: SWBF2 0x006c35c0
void* RedAllocFromHeapInternal(int heap, unsigned int size, int bTop)
{
    if (!isHeapObjInitialized)
    {
        Startup_RedInitHeap();
    }
    if (heap < 0 || heap >= maxheaps)
    {
        return 0;
    }

    HeapObj& h = heapobj[heap];
    unsigned int mask = ~(h.align - 1);
    unsigned int hdr = (h.align + sizeof(Tag) - 1) & mask;
    unsigned int need = ((size ? size : 1) + h.align - 1 + hdr) & mask;

    Tag* tag;
    if (bTop)
    {
        for (tag = h.free.last; tag; tag = tag->prev_)
        {
            if (gRedHeapCheck && !HeapCheckTagOk(h, tag))
            {
                HeapCheckReport(heap, tag);
                return 0;
            }
            if (need <= tag->size)
            {
                break;
            }
        }
    }
    else
    {
        for (tag = h.free.first; tag; tag = tag->next_)
        {
            if (gRedHeapCheck && !HeapCheckTagOk(h, tag))
            {
                HeapCheckReport(heap, tag);
                return 0;
            }
            if (need <= tag->size)
            {
                break;
            }
        }
    }
    if (!tag)
    {
        if (sRecursionDepth <= 0)
        {
            RedMemory::failHeap = heap;
            RedMemory::failSize = need;
        }
        return 0;
    }

    unsigned int rest = tag->size - need;
    if (rest < h.align + hdr)
    {
        TagRemoveList(&h.free, tag);
    }
    else if (bTop)
    {
        tag->size = rest;
        tag = (Tag*)((char*)tag + rest);
        tag->next_ = 0;
        tag->prev_ = 0;
        tag->size = need;
    }
    else
    {
        Tag* split = (Tag*)((char*)tag + need);
        split->next_ = 0;
        split->prev_ = 0;
        split->size = 0;
        split->size = tag->size - need;
        split->prev_ = tag->prev_;
        split->next_ = tag->next_;
        tag->size = need;
        if (split->next_)
        {
            split->next_->prev_ = split;
        }
        else
        {
            h.free.last = split;
        }
        if (split->prev_)
        {
            split->prev_->next_ = split;
        }
        else
        {
            h.free.first = split;
        }
    }
    TagAddList(&h.used, tag);
    if (gRedHeapCheck)
    {
        HeapCheckRecord(tag, hdr);
    }
    return (char*)tag + hdr;
}

// FUNCTION: SWBF2 0x006c3730
bool RedFreeToHeapInternal(int heap, void* block, unsigned int flags)
{
    (void)flags;
    if (!isHeapObjInitialized)
    {
        return true;
    }
    if (heap < 0 || heap >= maxheaps)
    {
        return false;
    }

    HeapObj& h = heapobj[heap];
    if (block < h.start || block > (char*)h.start + h.size)
    {
        return false;
    }
    unsigned int hdr = (h.align + sizeof(Tag) - 1) & ~(h.align - 1);
    Tag* tag = (Tag*)((char*)block - hdr);
    TagRemoveList(&h.used, tag);
    TagMergeIntoList(&h.free, tag);
    return true;
}

// FUNCTION: SWBF2 0x006c37b0
void TagAddList(TagList* list, Tag* tag)
{
    if (list->first)
    {
        list->first->prev_ = tag;
    }
    else
    {
        list->last = tag;
    }
    tag->prev_ = 0;
    tag->next_ = list->first;
    list->first = tag;
}

// FUNCTION: SWBF2 0x006c37f0
void TagMergeIntoList(TagList* list, Tag* tag)
{
    Tag* prev = 0;
    Tag* next = list->first;
    while (next && tag > next)
    {
        prev = next;
        next = next->next_;
    }
    tag->next_ = next;
    tag->prev_ = prev;

    if (next)
    {
        next->prev_ = tag;
        if ((char*)tag + tag->size == (char*)next)
        {
            tag->size += next->size;
            next = next->next_;
            tag->next_ = next;
            if (next)
            {
                next->prev_ = tag;
            }
            else
            {
                list->last = tag;
            }
        }
    }
    else
    {
        list->last = tag;
    }

    if (prev)
    {
        prev->next_ = tag;
        if ((char*)prev + prev->size == (char*)tag)
        {
            prev->next_ = next;
            prev->size += tag->size;
            if (next)
            {
                next->prev_ = prev;
            }
            else
            {
                list->last = prev;
            }
        }
    }
    else
    {
        list->first = tag;
    }
}

// FUNCTION: SWBF2 0x006c3880
void TagRemoveList(TagList* list, Tag* tag)
{
    if (tag->next_)
    {
        tag->next_->prev_ = tag->prev_;
    }
    else
    {
        list->last = tag->prev_;
    }
    if (tag->prev_)
    {
        tag->prev_->next_ = tag->next_;
    }
    else
    {
        list->first = tag->next_;
    }
    tag->next_ = 0;
    tag->prev_ = 0;
}

// FUNCTION: SWBF2 0x006c38d0
void* RedAllocFromHeap(int heap, unsigned int size, int bTop)
{
    void* p = RedAllocFromHeapInternal(heap, size, bTop);
    if (!p)
    {
        if (OnFailure)
        {
            OnFailure(heap, size);
        }
        return 0;
    }
    return p;
}

// FUNCTION: SWBF2 0x006c3900
int RedCreateHeap(void* start, void* end, int align, char const* name)
{
    const uintptr_t mask = ~(uintptr_t)(align - 1);
    char* s = (char*)(((uintptr_t)start + align - 1) & mask);
    char* e = (char*)((uintptr_t)end & mask);
    unsigned int size = e - s;
    if (size)
    {
        memset(s, 0, size);
    }

    int i;
    for (i = 0; i < maxheaps; ++i)
    {
        if (heapobj[i].size == 0)
        {
            break;
        }
    }
    if (i >= maxheaps)
    {
        return -1;
    }

    Tag* tag = (Tag*)s;
    tag->size = size;
    tag->next_ = 0;
    tag->prev_ = 0;

    HeapObj& heap = heapobj[i];
    heap.start = s;
    heap.free.first = tag;
    heap.free.last = tag;
    heap.size = size;
    heap.used.first = 0;
    heap.used.last = 0;
    heap.align = align;
    heap.name = (char*)name;
    heap.frozen = false;
    return i;
}

// FUNCTION: SWBF2 0x006c39b0
void RedDestroyHeap(int heap)
{
    HeapObj& h = heapobj[heap];
    h.free.first = 0;
    h.free.last = 0;
    h.used.first = 0;
    h.used.last = 0;
    h.size = 0;
    h.name = 0;
    h.align = 4;
    h.frozen = false;
}

// FUNCTION: SWBF2 0x006c3a00
void RedFreeToHeap(int heap, void* block, int flags)
{
    if (RedFreeToHeapInternal(heap, block, flags))
    {
        return;
    }
    for (int i = 0; i < maxheaps; ++i)
    {
        if (RedFreeToHeapInternal(i, block, flags))
        {
            return;
        }
    }
    free(block);
}

// FUNCTION: SWBF2 0x006c3aa0
char* RedInitHeap(void* start, void* end, int maxheaps)
{
    (void)end;
    ::maxheaps = maxheaps;
    heapobj = (HeapObj*)(((uintptr_t)start + 3) & ~(uintptr_t)3);
    char* pNext = (char*)(((uintptr_t)heapobj + maxheaps * sizeof(HeapObj) + 3) & ~(uintptr_t)3);
    for (int i = 0; i < maxheaps; ++i)
    {
        HeapObj& h = heapobj[i];
        h.start = 0;
        h.free.first = 0;
        h.free.last = 0;
        h.used.first = 0;
        h.used.last = 0;
        h.size = 0;
        h.align = 4;
        h.frozen = false;
        h.name = 0;
    }
    isHeapObjInitialized = true;
    gRedHeapCheck = strstr(GetCommandLineA(), "-heapcheck") != 0;
    return pNext;
}

// FUNCTION: SWBF2 0x006c3b20
void RedMarkHeapPermanent(int heap, bool permanent)
{
    heapobj[heap].permanent = permanent;
}

// FUNCTION: SWBF2 0x006c3c10
int RedSetCurrentHeap(int heap)
{
    if (heap >= 0 && heap < maxheaps)
    {
        int old = __RedCurrHeap;
        __RedCurrHeap = heap;
        return old;
    }
    return __RedCurrHeap;
}

// FUNCTION: SWBF2 0x006c3c40
void RedSetDebugHeap(int heap)
{
    __RedDebugHeap = heap;
}

// FUNCTION: SWBF2 0x006c3c50
void RedSetTempHeap(int heap)
{
    __RedTempHeap = heap;
}

// FUNCTION: SWBF2 0x006c3a80
unsigned int RedGetHeapSize(int heap)
{
    return heapobj[heap].size;
}
