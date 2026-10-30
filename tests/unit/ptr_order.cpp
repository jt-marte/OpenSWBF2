#include "third_party/doctest/doctest.h"

#include "Battlefront2/Source/Thread.h"
#include "PebbleFL/Common/PblPtrOrder.h"
#include <stdio.h>
#include <string>

#include "testutil.h"

namespace
{

void* Addr(unsigned int uiAddr)
{
    return (void*)(size_t)uiAddr;
}

bool ReadSource(const char* pcPath, std::string& sOut)
{
    FILE* pFile = fopen(pcPath, "rb");
    if (!pFile)
    {
        return false;
    }
    char acBuf[4096];
    size_t uiRead;
    while ((uiRead = fread(acBuf, 1, sizeof(acBuf), pFile)) > 0)
    {
        sOut.append(acBuf, uiRead);
    }
    fclose(pFile);
    return true;
}

}

TEST_CASE("ptr order key is monotonic across 2GB" * doctest::test_suite("unit"))
{
    const unsigned int auiAddr[] = { 0x00010000u, 0x7FFFFFF0u, 0x80000010u, 0x80000014u, 0xFFFEFFFCu };
    for (int i = 1; i < 5; ++i)
    {
        CHECK(PblPtrOrderKey(Addr(auiAddr[i - 1])) < PblPtrOrderKey(Addr(auiAddr[i])));
    }
    CHECK(PblPtrOrderKey(Addr(0xFFFEFFFCu)) < 0x7FFFFFFF);
}

TEST_CASE("ordered list keeps address order across 2GB" * doctest::test_suite("unit"))
{
    const unsigned int auiAddr[] = { 0x80000014u, 0x00010000u, 0xFFFEFFFCu, 0x7FFFFFF0u, 0x80000010u };
    PblOrderedList<Thread, int> list;
    PblOrderedList<Thread, int>::Node aNodes[5];
    for (int i = 0; i < 5; ++i)
    {
        list.Add(&aNodes[i], NULL, PblPtrOrderKey(Addr(auiAddr[i])));
    }

    const unsigned int auiSorted[] = { 0x00010000u, 0x7FFFFFF0u, 0x80000010u, 0x80000014u, 0xFFFEFFFCu };
    PblOrderedList<Thread, int>::Node* pNode = list._head._pNext;
    for (int i = 0; i < 5; ++i)
    {
        CHECK(pNode->_iOrder == PblPtrOrderKey(Addr(auiSorted[i])));
        pNode = pNode->_pNext;
    }
    CHECK(pNode == &list._head);
}

TEST_CASE("collision tie-break is unsigned" * doctest::test_suite("unit"))
{
    CHECK(PblPtrAfter(Addr(0x80000010u), Addr(0x7FFFFFF0u)));
    CHECK(!PblPtrAfter(Addr(0x7FFFFFF0u), Addr(0x80000010u)));
    CHECK(!PblPtrAfter(Addr(0x80000010u), Addr(0x80000010u)));
}

TEST_CASE("pointer order sites use the unsigned helpers" * doctest::test_suite("unit"))
{
    std::string sThread;
    std::string sCollision;
    std::string sMemory;
    if (!ReadSource("../src/Battlefront2/Source/Thread.cpp", sThread) ||
        !ReadSource("../src/Battlefront2/Source/CollisionManager.cpp", sCollision) ||
        !ReadSource("../src/RedEngineFL/Memory/RedMemory.cpp", sMemory))
    {
        SkipCase("source files not found");
    }

    CHECK(sThread.find("(int)this") == std::string::npos);
    CHECK(sThread.find("PblPtrOrderKey(this)") != std::string::npos);
    CHECK(sCollision.find("(int)pOther > (int)pObject") == std::string::npos);
    CHECK(sCollision.find("PblPtrAfter(pOther, pObject)") != std::string::npos);
    CHECK(sMemory.find("0x70000000") == std::string::npos);
    CHECK(sMemory.find("if (sRecursionDepth <= 0)") != std::string::npos);
}
