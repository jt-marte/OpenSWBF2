#include "NetWire.h"

#include <string.h>

namespace NetWire
{
static void Drain(NetPktGroup& group, unsigned int uCount)
{
    char acScratch[64];
    while (uCount > 0)
    {
        const unsigned int uStep = uCount < sizeof(acScratch) ? uCount : (unsigned int)sizeof(acScratch);
        group.ReadBytes(acScratch, (int)uStep);
        uCount -= uStep;
    }
}

static bool IsReservedDeviceName(const char* pcName)
{
    char acBase[8];
    unsigned int uLen = 0;
    while (pcName[uLen] && pcName[uLen] != '.')
    {
        if (uLen >= sizeof(acBase) - 1)
        {
            return false;
        }
        char cChar = pcName[uLen];
        if (cChar >= 'a' && cChar <= 'z')
        {
            cChar = (char)(cChar - 'a' + 'A');
        }
        acBase[uLen++] = cChar;
    }
    acBase[uLen] = 0;
    if (uLen == 3)
    {
        return strcmp(acBase, "CON") == 0 || strcmp(acBase, "PRN") == 0 || strcmp(acBase, "AUX") == 0 ||
               strcmp(acBase, "NUL") == 0;
    }
    if (uLen == 4)
    {
        return (strncmp(acBase, "COM", 3) == 0 || strncmp(acBase, "LPT", 3) == 0) && acBase[3] >= '1' &&
               acBase[3] <= '9';
    }
    return false;
}

static bool IsNameChar(char cChar, bool bAllowDot)
{
    return (cChar >= 'a' && cChar <= 'z') || (cChar >= 'A' && cChar <= 'Z') ||
           (cChar >= '0' && cChar <= '9') || cChar == '_' || cChar == '-' || (bAllowDot && cChar == '.');
}

bool ReadChatPayload(NetPktGroup& group, unsigned char* pDst, unsigned int uCap, unsigned int uSize)
{
    const unsigned int uCopy = uSize < uCap ? uSize : uCap;
    group.ReadBytes((char*)pDst, (int)uCopy);
    if (uSize > uCap)
    {
        Drain(group, uSize - uCap);
        NetRejectCount(NET_REJECT_CHAT_OVERSIZE);
    }
    if (uCap >= 2)
    {
        pDst[uCap - 1] = 0;
        pDst[uCap - 2] = 0;
    }
    return !group.IsBad();
}

bool IsValidChatFields(int iPlayerID, int iCode, bool bFromClient)
{
    const int iMaxID = bFromClient ? (int)kMaxPlayers - 1 : (int)kMaxPlayers;
    if (iPlayerID < 0 || iPlayerID > iMaxID || iCode < -1 || iCode > 7)
    {
        NetRejectCount(NET_REJECT_CHAT_FIELDS);
        return false;
    }
    return true;
}

bool IsSafeUploadName(const char* pcName)
{
    bool bOk = pcName && pcName[0] && pcName[0] != '.';
    unsigned int uLen = 0;
    while (bOk && pcName[uLen])
    {
        bOk = IsNameChar(pcName[uLen], true);
        ++uLen;
    }
    bOk = bOk && uLen < 16 && !IsReservedDeviceName(pcName);
    if (!bOk)
    {
        NetRejectCount(NET_REJECT_UPLOAD_NAME);
    }
    return bOk;
}

bool IsSafeUploadDir(const char* pcDir)
{
    bool bOk = pcDir && pcDir[0];
    for (unsigned int i = 0; bOk && pcDir[i]; ++i)
    {
        bOk = IsNameChar(pcDir[i], false);
    }
    bOk = bOk && !IsReservedDeviceName(pcDir) && _stricmp(pcDir, "data") != 0 &&
          _stricmp(pcDir, "addon") != 0;
    if (!bOk)
    {
        NetRejectCount(NET_REJECT_UPLOAD_NAME);
    }
    return bOk;
}

bool ReadUploadChunk(NetPktGroup& group, char* pDst, unsigned int uCap, unsigned int uSizeLeft,
                     unsigned int& uLengthOut)
{
    unsigned int uLength = 0;
    group.ReadBits(uLength, 10);
    uLengthOut = uLength;
    const unsigned int uCopy = uLength < uCap ? uLength : uCap;
    group.ReadBytes(pDst, (int)uCopy);
    if (uLength > uCopy)
    {
        Drain(group, uLength - uCopy);
    }
    if (group.IsBad())
    {
        return false;
    }
    if (uLength > uCap)
    {
        NetRejectCount(NET_REJECT_UPLOAD_LENGTH);
        return false;
    }
    if (uLength > uSizeLeft)
    {
        NetRejectCount(NET_REJECT_UPLOAD_UNDERFLOW);
        return false;
    }
    return true;
}

void DiscardUpload(NetPktGroup& group, bool bHeader)
{
    if (bHeader)
    {
        bool bMore = false;
        group.Read(bMore);
        if (bMore)
        {
            char acName[32];
            ReadString(group, acName, sizeof(acName));
            unsigned int uSize = 0;
            group.ReadBits(uSize, 32);
        }
        return;
    }
    char acBuffer[kUploadChunkMax];
    unsigned int uLength = 0;
    ReadUploadChunk(group, acBuffer, sizeof(acBuffer), 0xffffffffu, uLength);
}

int ReadRconLine(const char* pcData, int iAvail, unsigned int uLength, unsigned short* pusOut)
{
    unsigned int uCount = uLength;
    if (uCount > kRconLineMax)
    {
        uCount = kRconLineMax;
    }
    const unsigned int uAvail = iAvail > 0 ? (unsigned int)iAvail : 0;
    if (uCount > uAvail)
    {
        uCount = uAvail;
    }
    for (unsigned int i = 0; i < uCount; ++i)
    {
        pusOut[i] = (unsigned short)(short)pcData[i];
    }
    pusOut[uCount] = 0;
    if (uCount < uLength)
    {
        NetRejectCount(NET_REJECT_RCON_LINE);
    }
    return (int)uCount;
}

bool IsRconAuthorized(const char* pcData, int iAvail, char cKey0, char cKey1)
{
    if (iAvail < 7)
    {
        return false;
    }
    const unsigned char uCommand = (unsigned char)pcData[5];
    if (uCommand == ')' || uCommand == 'J')
    {
        if (iAvail < 8)
        {
            NetRejectCount(NET_REJECT_RCON_AUTH_SHORT);
            return false;
        }
        if (pcData[6])
        {
            return false;
        }
        const unsigned char uKey = (unsigned char)pcData[7];
        return uKey == (unsigned char)cKey0 || uKey == (unsigned char)cKey1;
    }
    return uCommand == 's' && pcData[6] == 0;
}

unsigned __int64 RconPeerKey(const void* pPeerRecord, bool bLAN)
{
    const unsigned char* pucRecord = (const unsigned char*)pPeerRecord;
    if (!bLAN)
    {
        unsigned __int64 uGalaxyId;
        memcpy(&uGalaxyId, pucRecord, sizeof(uGalaxyId));
        return uGalaxyId;
    }
    // sockaddr_in after the Galaxy ID: family (2 bytes), port (2, network order), address (4)
    const unsigned int uPort = ((unsigned int)pucRecord[10] << 8) | pucRecord[11];
    unsigned int uAddress;
    memcpy(&uAddress, pucRecord + 12, sizeof(uAddress));
    return (unsigned __int64)uAddress | ((unsigned __int64)uPort << 32);
}

unsigned int RconLoginKey(const void* pPeerRecord, bool bLAN)
{
    const unsigned __int64 uPeer = RconPeerKey(pPeerRecord, bLAN);
    const unsigned int uKey = bLAN ? (unsigned int)uPeer : (unsigned int)(uPeer ^ (uPeer >> 32));
    return uKey ? uKey : 0xFFFFFFFFu;
}

int PickRconSlot(const float* pafLastRecv, const bool* pabLoggedIn, int iCount, float fNow)
{
    for (int i = 0; i < iCount; ++i)
    {
        const float fLast = pafLastRecv[i];
        if (fLast == 0.0f || fLast > fNow || fNow - fLast > kRconSessionSeconds)
        {
            return i;
        }
    }
    int iSlot = -1;
    for (int i = 0; i < iCount; ++i)
    {
        if (!pabLoggedIn[i] && (iSlot < 0 || pafLastRecv[i] < pafLastRecv[iSlot]))
        {
            iSlot = i;
        }
    }
    return iSlot;
}

LoginLimiter::LoginLimiter()
{
    memset(maEntries, 0, sizeof(maEntries));
}

bool LoginLimiter::Allow(unsigned int uKey, float fNow)
{
    if (uKey == 0)
    {
        return true;
    }
    int iSlot = -1;
    int iOldest = 0;
    for (int i = 0; i < 16; ++i)
    {
        if (maEntries[i].uKey == uKey)
        {
            iSlot = i;
            break;
        }
        if (maEntries[i].uKey == 0)
        {
            iOldest = i;
            break;
        }
        if (maEntries[i].fWindowStart < maEntries[iOldest].fWindowStart)
        {
            iOldest = i;
        }
    }
    if (iSlot < 0)
    {
        maEntries[iOldest].uKey = uKey;
        maEntries[iOldest].fWindowStart = fNow;
        maEntries[iOldest].iCount = 1;
        return true;
    }
    Entry& entry = maEntries[iSlot];
    if (fNow < entry.fWindowStart || fNow - entry.fWindowStart >= 10.0f)
    {
        entry.fWindowStart = fNow;
        entry.iCount = 1;
        return true;
    }
    if (entry.iCount >= 5)
    {
        NetRejectCount(NET_REJECT_RCON_LOGIN_RATE);
        return false;
    }
    ++entry.iCount;
    return true;
}

bool IsValidGuestCount(unsigned int uGuests)
{
    if (uGuests > kMaxGuests)
    {
        NetRejectCount(NET_REJECT_GUEST_COUNT);
        return false;
    }
    return true;
}

// The LAN FOUND reply packs the player limit into 6 bits, so a 64-player host sends 0. Limits run
// 1..64, and the joining client sizes its character table from this value, which must match the host.
int DecodeLanMaxPlayers(unsigned int uWire)
{
    return uWire == 0 ? (int)kMaxPlayers : (int)uWire;
}

// Upload cap in "slots" of 40 kbit/s, the unit DoThrottle multiplies by. Retail stopped at
// 153 slots (6M); the last entry is about 2 Gbit/s, which no link reaches.
int HostBandwidthSlots(unsigned int uOption)
{
    static const int aiSlots[kHostBandwidthOptions] = {3, 6, 9, 12, 19, 25, 38, 76, 153, 256, 640, 1280, 2560, 48828};
    return aiSlots[uOption < kHostBandwidthOptions ? uOption : kHostBandwidthOptions - 1];
}

int HostTurnRate(int iProfileTurns)
{
    if (iProfileTurns < 30)
    {
        return kDefaultTurnRate;
    }
    return iProfileTurns > 255 ? 255 : iProfileTurns;
}

int ServerBotSlots(int iStartBots)
{
    return iStartBots > kRetailBotSlots ? kMaxBotSlots : kRetailBotSlots;
}

int ServerBotCap(int iModeBots, int iSlots)
{
    if (iModeBots < 0)
    {
        return 0;
    }
    return iModeBots > iSlots ? iSlots : iModeBots;
}

// Only ever raised, never lowered: a client keeps the map's own count, and twice this never
// exceeds the other term of CharacterCount (players + 2 * slots), so both sides still count the
// same characters.
int ServerTeamUnits(int iMissionUnits, int iPlayerLimit, int iSlots)
{
    int iUnits = (iPlayerLimit + 2 * iSlots) / 2;
    if (iUnits > kServerTeamCap)
    {
        iUnits = kServerTeamCap;
    }
    return iMissionUnits > iUnits ? iMissionUnits : iUnits;
}

int ServerClassMax(int iClassMax, int iMissionUnits, int iServerUnits)
{
    if (iClassMax <= 0 || iMissionUnits <= 0 || iServerUnits <= iMissionUnits)
    {
        return iClassMax;
    }
    const long long llMax = ((long long)iClassMax * iServerUnits + iMissionUnits - 1) / iMissionUnits;
    return llMax > 0x7fffffff ? 0x7fffffff : (int)llMax;
}

int CharacterCount(int iUnits1, int iUnits2, bool bNet, int iPlayerLimit, int iBots, int iOthers)
{
    int iCount = iUnits1 + iUnits2;
    if (bNet && iPlayerLimit + 2 * iBots > iCount)
    {
        iCount = iPlayerLimit + 2 * iBots;
    }
    iCount += iOthers;
    return iCount > 600 ? 600 : iCount;
}

// Any unicast IPv4 a LAN-transport peer can use: private, VPN (100.64/10), public and loopback.
// Retail accepted only 10/8, 172.16/12 and 192.168/16, which kept Direct and VPN play out.
bool IsUsableIPv4(unsigned long ulNetOrder)
{
    const unsigned int uFirst = (unsigned int)(ulNetOrder & 0xff);
    if (ulNetOrder == 0 || ulNetOrder == 0xffffffffu)
    {
        return false;
    }
    return uFirst != 0 && uFirst < 224;
}

// "host", "host:port", with surrounding spaces. Host names and dotted IPv4 only: letters,
// digits, '.' and '-'. The port must be 1..65535; without one, iDefaultPort is used.
bool ParseHostPort(const char* pcText, char* pcHost, unsigned int uHostCap, int& iPort, int iDefaultPort)
{
    if (!pcText || !pcHost || uHostCap < 2)
    {
        return false;
    }
    while (*pcText == ' ' || *pcText == '	')
    {
        ++pcText;
    }
    unsigned int uLen = 0;
    while (pcText[uLen] && pcText[uLen] != ':' && pcText[uLen] != ' ' && pcText[uLen] != '	')
    {
        const char c = pcText[uLen];
        const bool bOk = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                         c == '-';
        if (!bOk || uLen + 1 >= uHostCap)
        {
            return false;
        }
        pcHost[uLen] = c;
        ++uLen;
    }
    if (!uLen)
    {
        return false;
    }
    pcHost[uLen] = 0;
    const char* pcRest = pcText + uLen;
    iPort = iDefaultPort;
    if (*pcRest == ':')
    {
        ++pcRest;
        int iValue = 0;
        int iDigits = 0;
        while (*pcRest >= '0' && *pcRest <= '9')
        {
            iValue = iValue * 10 + (*pcRest - '0');
            if (++iDigits > 5)
            {
                return false;
            }
            ++pcRest;
        }
        if (!iDigits || iValue < 1 || iValue > 65535)
        {
            return false;
        }
        iPort = iValue;
    }
    while (*pcRest == ' ' || *pcRest == '	')
    {
        ++pcRest;
    }
    return *pcRest == 0;
}

int ClampPlayerCount(unsigned int uWire)
{
    int iCount = uWire > kMaxPlayers ? (int)kMaxPlayers : (int)uWire;
    if (iCount < 1)
    {
        iCount = 1;
    }
    if ((unsigned int)iCount != uWire)
    {
        NetRejectCount(NET_REJECT_PLAYER_COUNT);
    }
    return iCount;
}

bool IsValidTurnRate(int iDuration, int iTurns)
{
    if (iTurns < 1 || iTurns > 7 || iDuration < 1 || iDuration > 255 || iDuration / iTurns < 1)
    {
        NetRejectCount(NET_REJECT_TURN_RATE);
        return false;
    }
    return true;
}

void WriteString(NetPktGroup& group, const char* pcText, unsigned int uCapacity)
{
    unsigned int uLength = (unsigned int)strlen(pcText);
    if (uLength >= uCapacity)
    {
        uLength = uCapacity - 1;
    }
    group.WriteBits(uLength, 8);
    for (unsigned int i = 0; i < uLength; ++i)
    {
        group.WriteBits(pcText[i], 8);
    }
}

void ReadString(NetPktGroup& group, char* pcText, unsigned int uCapacity)
{
    unsigned int uLength = 0;
    group.ReadBits(uLength, 8);
    for (unsigned int i = 0; i < uLength; ++i)
    {
        unsigned int uValue = 0;
        group.ReadBits(uValue, 8);
        if (i < uCapacity)
        {
            pcText[i] = (char)uValue;
        }
    }
    pcText[uCapacity - 1] = 0;
    if (uLength < uCapacity - 1)
    {
        pcText[uLength] = 0;
    }
}

void WriteChatPayload(NetPktGroup& group, const void* pMsg, unsigned int uLen)
{
    if (uLen == 0)
    {
        const unsigned int uEmpty = 0xffff0000;
        group.WriteBytes((const char*)&uEmpty, sizeof(uEmpty));
        return;
    }
    const unsigned int uHeader = uLen | 0xffff0000;
    group.WriteBytes((const char*)&uHeader, sizeof(uHeader));
    group.WriteBytes((const char*)pMsg, (int)uLen);
}

void WriteUploadHeader(NetPktGroup& group, unsigned int uSeq, bool bMore, const char* pcName,
                       unsigned int uSize)
{
    group.Write(true);
    group.WriteBits(uSeq, 32);
    group.Write(bMore);
    if (bMore)
    {
        WriteString(group, pcName, 16);
        group.WriteBits(uSize, 32);
    }
}

void WriteUploadChunk(NetPktGroup& group, unsigned int uSeq, const char* pData, unsigned int uLen)
{
    group.Write(false);
    group.WriteBits(uSeq, 32);
    group.WriteBits(uLen, 10);
    group.WriteBytes(pData, (int)uLen);
}

void WriteTurnRate(NetPktGroup& group, unsigned int uRate, unsigned int uScale)
{
    group.WriteBits(uRate, 10);
    group.WriteBits(uScale, 3);
}

void WritePlayerCount(NetPktGroup& group, unsigned int uCount)
{
    group.WriteBits(uCount, 7);
}
}
