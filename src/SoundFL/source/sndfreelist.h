#pragma once

#include "sndallocator.h"
#include "sndallocbitmap.h"
#include "PebbleFL/Common/PblListSingle.h"

// A free list's entry size: retail's Win32 size, and on x64 at least the type's own size.
#ifdef _WIN64
#define SND_FREELIST_SIZE(uiWin32Size, Type) (sizeof(Type) > (uiWin32Size) ? (unsigned int)sizeof(Type) : (unsigned int)(uiWin32Size))
#else
#define SND_FREELIST_SIZE(uiWin32Size, Type) (uiWin32Size)
#endif

namespace Snd
{

class FreeList
{
public:

    class Block : public Allocator<Block>
    {
    public:

        Block(void* pAllocAddress)
        {
            mNode.mNext = 0;
            mBitmap.mBitmap = 0;
            mBitmap.mBitmapSize = 0;
            mBitmap.mSearchEntry = 0;
            mAllocAddress = pAllocAddress;
        }

        void* AllocEntry(unsigned int uiEntrySize, unsigned int uiEntriesPerBlock);

        PblListSingle::Node mNode;
        AllocBitmap mBitmap;
        void* mAllocAddress;
    };

    FreeList(unsigned int uiEntrySize, unsigned int uiAlignment,
             unsigned int uiEntriesPerBlock, unsigned char ucFlags)
        : mEntrySize(uiEntrySize),
          mAlignment(uiAlignment),
          mEntriesPerBlock(uiEntriesPerBlock),
          mAllocFunc(0),
          mFreeFunc(0),
          mFlags(ucFlags)
    {
    }

    enum Flags
    {
        flags_purgeOnFree = 1,
        flags_locked = 2
    };

    void* Alloc();
    bool AllocBlocks(unsigned int uiNumBlocks,
                     Block** ppFirstBlock);
    void Free(void* p);
    void PurgeBlocks();

    unsigned int mEntrySize;
    unsigned int mAlignment;
    unsigned int mEntriesPerBlock;
    PblListSingle mBlockList;
    void* (__cdecl* mAllocFunc)(size_t);
    void (__cdecl* mFreeFunc)(void*);
    unsigned char mFlags;
};

 
 

}
