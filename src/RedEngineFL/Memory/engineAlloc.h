#ifndef ENGINE_ALLOC_H
#define ENGINE_ALLOC_H

void* __cdecl operator new(size_t size);
void __cdecl operator delete(void* block);
void* __cdecl operator new[](size_t size);
void __cdecl operator delete[](void* block);

extern "C" int __RedCurrHeap;

#endif
