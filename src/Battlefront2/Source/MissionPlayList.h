#pragma once

namespace MissionPlayList
{

struct PlaylistEntry
{
    char mLuaFile[64];
    unsigned char mSide;
    unsigned int mTeam1Hash;
    unsigned int mTeam2Hash;
    int mTickets1;
    int mTickets2;
};

void ClearEntries();
void SetEntry(char const* pcLuaFile, unsigned char uSide, unsigned int uiTeam1Hash,
              unsigned int uiTeam2Hash, int iTickets1, int iTickets2);
bool AddEntry(char const* pcLuaFile, unsigned char uSide, char const* pcTeam1,
              char const* pcTeam2, int iTickets1, int iTickets2);
void SetLoopForever(bool bLoop);
int GetCount();
void RandomizeOrder();

int GetMaxSize();
bool PeekNextEntry(char const*& pcLuaFile, unsigned int& uiTeam1Hash,
                   unsigned int& uiTeam2Hash);
bool SelectNextEntry();
void GetCurrentTeamNames(unsigned int& uiTeam1Hash, unsigned int& uiTeam2Hash);
void SetCurrentTeamNames(unsigned int uiTeam1Hash, unsigned int uiTeam2Hash);
void SetClientEntry(char const* pcLuaFile, unsigned int uiTeam1Hash,
                    unsigned int uiTeam2Hash);
void PushToLua();

int GetEntryCount();
int GetCurrentEntry();
void GetEntry(int iEntry, char const*& pcLuaFile, int& iTickets1, int& iTickets2);
void AddEntry(char const* pcLuaFile, int iTickets1, int iTickets2);
bool DropEntry(char const* pcLuaFile);
bool NextEntry(char const* pcLuaFile);
unsigned int GetFirstMission();
unsigned int GetNextMission();
extern int mFindIndex;

// OpenSWBF2: rotation edits while a server runs (rcon /clearmaps, /appendmap, /movemap, /removemap,
// /maplist; MissionPlayListOps.h). Indices are 0-based.
// Empties the rotation for a new list: the running map plays on, the first appended entry plays
// next, and with nothing appended the running map replays.
void ClearForRebuild();
// Adds at the end; the entry's index, or -1 when the rotation is full.
int AppendEntry(char const* pcLuaFile, int iTickets1, int iTickets2);
// Both keep list order: when the running map is listed, the entry after it plays next.
bool MoveEntry(int iFrom, int iTo);
bool RemoveEntry(int iIndex);
// Read-only: unlike PeekNextEntry, these never wrap or shuffle the rotation.
int GetNextEntryIndex();
// The running map's entry, or -1 once ClearForRebuild has taken it out of the list.
int GetCurrentListedIndex();

}
