#pragma once

// OpenSWBF2: the Direct Connect tab's recent servers, newest first, with no engine
// dependencies so the unit tests can call it. recentservers.txt (next to the exe) holds one
// "address<TAB>name" line per server.
namespace RecentServers
{
const int kMaxServers = 8;
const int kAddrMax = 64; // as ScriptCB_BeginJoinIP reads an address
const int kNameMax = 64;
const int kTextMax = kMaxServers * (kAddrMax + kNameMax + 2) + 1;

struct Entry
{
    char acAddr[kAddrMax];
    char acName[kNameMax];
};

struct List
{
    Entry maEntries[kMaxServers];
    int miCount;
};

void Clear(List& list);
// Trims spaces, lowercases the host and drops the default ":3658", so "1.2.3.4" and
// "1.2.3.4:3658" are one server. False when nothing is left or it holds a space, tab or
// control character.
bool Normalize(const char* pcAddr, char* pcOut, int iOutSize);
// Puts the server first. One already listed moves up and keeps its old name when pcName is
// empty. False for an address Normalize rejects.
bool Add(List& list, const char* pcAddr, const char* pcName);
// Reads the file's text into list; bad, duplicate and extra lines are skipped.
void Parse(const char* pcText, List& list);
// Writes list as the file's text; returns its length, or -1 when iOutSize is too small.
int Serialize(const List& list, char* pcOut, int iOutSize);
}
