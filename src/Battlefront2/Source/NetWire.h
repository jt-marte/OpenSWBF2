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
// OpenSWBF2: rcon sessions. A peer record (NetSocketPeerRecord) is an 8-byte Galaxy ID, then a
// sockaddr_in. On LAN, and so on -dedicated, every Galaxy ID is 0: retail's compare of those 8
// bytes put every sender in one admin session, so one login authorized anyone. The socket
// address tells peers apart there.
const int kRconSlots = 3;
const float kRconSessionSeconds = 60.0f;
unsigned __int64 RconPeerKey(const void* pPeerRecord, bool bLAN);
// The login rate limit's key: the sender's IP on LAN (all its ports share one limit), the Galaxy
// ID online; never 0, which LoginLimiter treats as "no limit".
unsigned int RconLoginKey(const void* pPeerRecord, bool bLAN);
// The admin slot a new rcon peer takes: a free, stale or future-dated one first, then the oldest
// that isn't logged in. -1 when every slot holds a live login: a flood of heartbeats must not push
// an admin out.
int PickRconSlot(const float* pafLastRecv, const bool* pabLoggedIn, int iCount, float fNow);
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
// A dedicated server's bots. Its character table counts the same bot slots a team on every map
// (netNumBots, which clients take when they join), set at start from /bots: 32, retail's, which
// clients before 0.2.6 also end up with (their options clamp to 32), or 64 for more than 32, which
// takes 0.2.6+ clients. Each map's mode setting (/conaiperteam ...) only caps how many of them
// spawn, on the server. With 64 slots a team takes up to kServerTeamCap soldiers, humans and bots,
// so 64 players leave room for 64 bots in all: host and clients raise the map's unit count alike,
// since each creates a team's characters up to it (SpawnManager::InitialUpdate).
const int kRetailBotSlots = 32;
const int kMaxBotSlots = 64;
const int kServerTeamCap = 64;
int ServerBotSlots(int iStartBots);
// /bots on the command line fills a mode's bots a team only where the server's settings file
// (loaded first) left it unset (-1): launcher and Docker servers start with /bots 64 for the room
// and keep each mode's own count from ServerSettings.cfg
int StartModeBots(int iModeBots, int iStartBots);
int ServerBotCap(int iModeBots, int iSlots);
int ServerTeamUnits(int iMissionUnits, int iPlayerLimit, int iSlots);
// A unit class's most at once (AddUnitClass), grown with its team so the classes can fill it
int ServerClassMax(int iClassMax, int iMissionUnits, int iServerUnits);
// SpawnManager::GetTotalCharacterCount: host and clients must agree on it exactly
int CharacterCount(int iUnits1, int iUnits2, bool bNet, int iPlayerLimit, int iBots, int iOthers);

void WriteString(NetPktGroup& group, const char* pcText, unsigned int uCapacity);
void ReadString(NetPktGroup& group, char* pcText, unsigned int uCapacity);
void WriteChatPayload(NetPktGroup& group, const void* pMsg, unsigned int uLen);
void WriteUploadHeader(NetPktGroup& group, unsigned int uSeq, bool bMore, const char* pcName,
                       unsigned int uSize);
void WriteUploadChunk(NetPktGroup& group, unsigned int uSeq, const char* pData, unsigned int uLen);
void WriteTurnRate(NetPktGroup& group, unsigned int uRate, unsigned int uScale);
void WritePlayerCount(NetPktGroup& group, unsigned int uCount);
}
