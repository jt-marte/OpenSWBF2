#pragma once

// GLOBAL: SWBF2 0x0079f808
static const char sSideNamePrefixes[][4] = { "Neu", "All", "Imp", "Rep", "CIS", "Loc" };

static const char sSideNameNone[1] = { 0 };

inline const char* SideNamePrefix(int iSide)
{
    if (iSide >= 0 && iSide < (int)(sizeof(sSideNamePrefixes) / sizeof(sSideNamePrefixes[0])))
    {
        return sSideNamePrefixes[iSide];
    }
    return sSideNameNone;
}
