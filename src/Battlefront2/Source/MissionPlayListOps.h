#pragma once

// OpenSWBF2: index arithmetic for editing the rotation while a server runs (rcon /movemap,
// /removemap). The play position is the index of the next entry; position == count means the
// rotation wraps to the first entry. No engine dependencies, so the unit tests can call it.
namespace MissionPlayListOps
{
// Where the entry at iIndex ends up when the entry at iFrom moves to iTo (all 0-based).
inline int MovedIndex(int iIndex, int iFrom, int iTo)
{
    if (iIndex == iFrom)
    {
        return iTo;
    }
    if (iFrom < iTo && iIndex > iFrom && iIndex <= iTo)
    {
        return iIndex - 1;
    }
    if (iTo < iFrom && iIndex >= iTo && iIndex < iFrom)
    {
        return iIndex + 1;
    }
    return iIndex;
}

// The play position after a move: the same entry stays next, and a wrap stays a wrap.
inline int PositionAfterMove(int iPosn, int iCount, int iFrom, int iTo)
{
    return iPosn >= iCount ? iPosn : MovedIndex(iPosn, iFrom, iTo);
}

// The play position after removing iRemoved: entries before it shift down. Removing the next
// entry makes the one after it next.
inline int PositionAfterRemove(int iPosn, int iRemoved)
{
    return iRemoved < iPosn ? iPosn - 1 : iPosn;
}

// The 0-based index of the entry that plays next, or -1 for an empty rotation.
inline int NextIndex(int iPosn, int iCount)
{
    if (iCount <= 0)
    {
        return -1;
    }
    return iPosn < iCount ? iPosn : 0;
}
}
