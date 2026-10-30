#pragma once

namespace GameMemory
{

// OpenSWBF2: x64 objects are larger (8-byte pointers and their padding), so the x64 build doubles
// the arena and the heaps carved from it. Win32 keeps the retail sizes.
#ifdef _WIN64
const unsigned int kHeapScale = 2;
#else
const unsigned int kHeapScale = 1;
#endif
const unsigned int kArenaSize = 0x4000000 * kHeapScale;

void InitApplicationMemory(char* pStart, char* pEnd);

void BuildHeaps(bool bMission);

void ReleaseTempHeap();

extern int ApplicationHeap;
extern int GamespyHeap;
extern int RunTimeHeap;
extern int TempLoadHeap;
extern int LuaHeap;
extern int ArtistHeap;
extern int TEMP_GamespyHeap;
}
