#pragma once

#include "NetPacket.h"

namespace NetWire
{
const unsigned int kChatMax = 276;
const unsigned int kUploadChunkMax = 1012;
const unsigned int kRconLineMax = 63;
const unsigned int kMaxPlayers = 64;
// PC joins carry at most one guest; the JOIN packet has room to claim seven.
const unsigned int kMaxGuests = 1;

struct LoginLimiter
{
    struct Entry
    {
        unsigned int uKey;
        float fWindowStart;
        int iCount;
    };

    LoginLimiter();
    bool Allow(unsigned int uKey, float fNow);

    Entry maEntries[16];
};

bool ReadChatPayload(NetPktGroup& group, unsigned char* pDst, unsigned int uCap, unsigned int uSize);
bool IsValidChatFields(int iPlayerID, int iCode, bool bFromClient);
bool IsSafeUploadName(const char* pcName);
bool IsSafeUploadDir(const char* pcDir);
bool ReadUploadChunk(NetPktGroup& group, char* pDst, unsigned int uCap, unsigned int uSizeLeft,
                     unsigned int& uLengthOut);
void DiscardUpload(NetPktGroup& group, bool bHeader);
int ReadRconLine(const char* pcData, int iAvail, unsigned int uLength, unsigned short* pusOut);
bool IsRconAuthorized(const char* pcData, int iAvail, char cKey0, char cKey1);
int ClampPlayerCount(unsigned int uWire);
bool IsValidGuestCount(unsigned int uGuests);
int DecodeLanMaxPlayers(unsigned int uWire);
bool IsUsableIPv4(unsigned long ulNetOrder);
// Host bandwidth option (profile mu8PlayersSupported): 0..8 are retail's 128K..6M, then
// OpenSWBF2's 10M, 25M, 50M, 100M and Unlimited.
const unsigned int kHostBandwidthOptions = 14;
const unsigned int kHostBandwidthDefault = 9; // 10M
int HostBandwidthSlots(unsigned int uOption);
// Hosting turn rate: the menu offers 30, 60 and 120; a profile still holding retail's 15 or 20
// (or nothing) hosts at the default.
const int kDefaultTurnRate = 60;
int HostTurnRate(int iProfileTurns);
bool ParseHostPort(const char* pcText, char* pcHost, unsigned int uHostCap, int& iPort, int iDefaultPort);
bool IsValidTurnRate(int iDuration, int iTurns);

void WriteString(NetPktGroup& group, const char* pcText, unsigned int uCapacity);
void ReadString(NetPktGroup& group, char* pcText, unsigned int uCapacity);
void WriteChatPayload(NetPktGroup& group, const void* pMsg, unsigned int uLen);
void WriteUploadHeader(NetPktGroup& group, unsigned int uSeq, bool bMore, const char* pcName,
                       unsigned int uSize);
void WriteUploadChunk(NetPktGroup& group, unsigned int uSeq, const char* pData, unsigned int uLen);
void WriteTurnRate(NetPktGroup& group, unsigned int uRate, unsigned int uScale);
void WritePlayerCount(NetPktGroup& group, unsigned int uCount);
}
