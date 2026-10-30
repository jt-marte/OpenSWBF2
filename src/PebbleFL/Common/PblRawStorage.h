#pragma once

// OpenSWBF2: some classes keep a member object in raw byte storage, sized as retail laid it out
// for Win32 and constructed with placement new. x64 objects are larger and hold 8-byte pointers,
// so x64 gets its own size and 8-byte alignment while Win32 keeps the retail layout. Each
// placement new checks its storage with a static_assert.
#ifdef _WIN64
#define PBL_RAW_SIZE(uiWin32Size, uiX64Size) (uiX64Size)
#define PBL_RAW_ALIGN __declspec(align(8))
#else
#define PBL_RAW_SIZE(uiWin32Size, uiX64Size) (uiWin32Size)
#define PBL_RAW_ALIGN
#endif
