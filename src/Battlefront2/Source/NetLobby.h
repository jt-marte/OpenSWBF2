#pragma once

// OpenSWBF2: a dedicated server's lobby is its pre-game held open until an admin starts the match
// (-lobby, rcon /start). Pre-game already keeps the AI out and stops timers, tickets and captures
// on every client, and its time reaches clients in 10 bits: the top value marks the lobby, so a
// client tells it from a countdown without a new packet field. No engine dependencies, so the unit
// tests can call it.
namespace NetLobby
{
const int kLobbyPreGameTime = 1023;
const int kMaxPreGameTime = 1000; // a real countdown never reaches the lobby marker

inline bool IsLobby(bool bPreGame, int iPreGameTime)
{
    return bPreGame && iPreGameTime == kLobbyPreGameTime;
}

inline int ClampPreGameTime(int iSeconds)
{
    return iSeconds < 0 ? 0 : (iSeconds > kMaxPreGameTime ? kMaxPreGameTime : iSeconds);
}
}
