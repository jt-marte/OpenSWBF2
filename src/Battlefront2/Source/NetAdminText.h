#pragma once

// OpenSWBF2: text for the server manager's rcon replies. Replies are split into lines on '\n' and
// each line is sent 127 characters at most, so a player's name must neither break a line nor run
// on. No engine dependencies, so the unit tests can call it.
namespace NetAdminText
{
const unsigned int kReplyLineMax = 127;
const unsigned int kMissionNameMax = 63; // MissionPlayList::PlaylistEntry::mLuaFile

// A player name made safe for one reply line: control characters become '?', and it is cut to
// fit uCap (terminator included).
inline void CleanName(const char* pcName, char* pcOut, unsigned int uCap)
{
    if (!uCap)
    {
        return;
    }
    unsigned int uLen = 0;
    for (const char* pc = pcName ? pcName : ""; *pc && uLen + 1 < uCap; ++pc)
    {
        const unsigned char c = (unsigned char)*pc;
        pcOut[uLen++] = (c < 0x20 || c == 0x7f) ? '?' : (char)c;
    }
    pcOut[uLen] = 0;
}

// A mission name an admin may put in the playlist: letters, digits and '_' only. A bad name
// would only fail when its turn comes to load.
inline bool IsMissionName(const char* pcName)
{
    if (!pcName || !*pcName)
    {
        return false;
    }
    unsigned int uLen = 0;
    for (const char* pc = pcName; *pc; ++pc, ++uLen)
    {
        const char c = *pc;
        const bool bOk = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!bOk || uLen >= kMissionNameMax)
        {
            return false;
        }
    }
    return true;
}
}
