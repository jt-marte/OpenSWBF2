#pragma once

#include <stddef.h>

// Flipping the top bit turns unsigned address order into signed key order. Pointer-sized,
// so int on Win32.
inline intptr_t PblPtrOrderKey(const void* pPtr)
{
    return (intptr_t)((uintptr_t)pPtr ^ ((uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1)));
}

inline bool PblPtrAfter(const void* pA, const void* pB)
{
    return (uintptr_t)pA > (uintptr_t)pB;
}

// The debug heap's freed-memory fill (0xDD in every byte) read back as a pointer.
const uintptr_t kPblFreedPtr = (uintptr_t)-1 / 0xFF * 0xDD;
