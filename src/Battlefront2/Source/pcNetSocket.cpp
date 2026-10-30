#include <winsock2.h>

#include "pcNetSocket.h"
#include <string.h>
#include "NetPacket.h"
#include "NetComm.h"
#include "Steam/galaxy_api.h"
#include "RedEngineFL/Util/RedLog.h"

extern unsigned int netDirectSocket;
extern unsigned int netBroadcastSendSocket;
extern unsigned int netBroadcastRecvSocket;
extern int hostshort;

extern int netSocketErrorCount;
extern bool gNetLoopback;

// OpenSWBF2 -netloopback: sockets listen on 127.0.0.1 only, so only this PC can reach them (Direct
// joins to 127.0.0.1; LAN discovery cannot work). Windows Firewall does not prompt for a loopback
// socket, which matters for test runs that start the exe from fresh folders.
static unsigned long ListenAddress()
{
    return gNetLoopback ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
}

// FUNCTION: SWBF2 0x00618540
void NetSocket::GetAddr(const void* pPeer, int& iIP, int& iPort)
{
    const NetSocketPeerRecord* pRecord = (const NetSocketPeerRecord*)pPeer;
    const sockaddr_in& addr = *(const sockaddr_in*)pRecord->mSocketAddress;
    iIP = (int)addr.sin_addr.s_addr;
    iPort = ntohs(addr.sin_port);
}

// FUNCTION: SWBF2 0x00618590
void DecodeError(int iError)
{
    switch (iError)
    {
    case 10022: case 10035: case 10038: case 10048:
    case 10057: case 10065: case 10093:
        ++netSocketErrorCount;
        break;
    }
}

// FUNCTION: SWBF2 0x00617bf0
void NetSocket::StartSockets()
{

    WSADATA wsaData;
    WSAStartup(0x0202, &wsaData);
}

// FUNCTION: SWBF2 0x00617c10
void NetSocket::OpenBroadcastSockets()
{
    sockaddr_in addr;
    BOOL bBroadcast = TRUE;
    netBroadcastSendSocket = (unsigned int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt((SOCKET)netBroadcastSendSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&bBroadcast,
               sizeof(bBroadcast));
    if (gNetLoopback)
    {
        // connect would bind it to any address, so bind it to loopback first
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        bind((SOCKET)netBroadcastSendSocket, (const sockaddr*)&addr, sizeof(addr));
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_BROADCAST;
    addr.sin_port = htons(3656);
    if (connect((SOCKET)netBroadcastSendSocket, (const sockaddr*)&addr, sizeof(addr)) < 0)
    {
        DecodeError(WSAGetLastError());
    }

    BOOL bReuse = TRUE;
    netBroadcastRecvSocket = (unsigned int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt((SOCKET)netBroadcastRecvSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&bReuse,
               sizeof(bReuse));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ListenAddress();
    addr.sin_port = htons(3656);
    if (bind((SOCKET)netBroadcastRecvSocket, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        LOG(LogNet, Warning, "OPENSWBF2 NET broadcast bind failed port=3656 err=%d", WSAGetLastError());
    }
    else
    {
        LOG(LogNet, Inform, "OPENSWBF2 NET broadcast open port=3656");
    }
    u_long ulNonBlocking = 1;
    ioctlsocket((SOCKET)netBroadcastRecvSocket, FIONBIO, &ulNonBlocking);
}

// FUNCTION: SWBF2 0x00617d40
int NetSocket::OpenDirectSocket(bool bHost)
{
    netDirectSocket = (unsigned int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int iPort = hostshort;
    if (!bHost)
    {
        ++iPort;
    }
    if ((SOCKET)netDirectSocket == INVALID_SOCKET)
    {
        LOG(LogNet, Error, "OPENSWBF2 NET socket failed err=%d", WSAGetLastError());
        netDirectSocket = 0;
        return iPort;
    }
    // Retail tried ports upward with no end; stop after 64 and let the system pick one.
    const int iFirstPort = iPort;
    for (;;)
    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ListenAddress();
        addr.sin_port = htons((u_short)iPort);
        if (bind((SOCKET)netDirectSocket, (const sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR)
        {
            break;
        }
        DecodeError(WSAGetLastError());
        if (iPort == 0)
        {
            LOG(LogNet, Error, "OPENSWBF2 NET socket bind failed err=%d", WSAGetLastError());
            break;
        }
        if (++iPort - iFirstPort >= 64)
        {
            LOG(LogNet, Warning, "OPENSWBF2 NET no free port in %d..%d, using any", iFirstPort, iPort - 1);
            iPort = 0;
        }
    }
    if (iPort == 0)
    {
        sockaddr_in bound;
        int iLen = sizeof(bound);
        if (getsockname((SOCKET)netDirectSocket, (sockaddr*)&bound, &iLen) != SOCKET_ERROR)
        {
            iPort = ntohs(bound.sin_port);
        }
    }
    LOG(LogNet, Inform, "OPENSWBF2 NET socket open host=%d port=%d%s", bHost ? 1 : 0, iPort,
        gNetLoopback ? " loopback=1" : "");
    int iSize = 0x8000;
    setsockopt((SOCKET)netDirectSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&iSize, sizeof(iSize));
    iSize = 0x10000;
    setsockopt((SOCKET)netDirectSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&iSize, sizeof(iSize));
    u_long ulNonBlocking = 1;
    ioctlsocket((SOCKET)netDirectSocket, FIONBIO, &ulNonBlocking);
    return iPort;
}

// FUNCTION: SWBF2 0x00617e30
void NetSocket::CloseDirectSocket()
{
    closesocket((SOCKET)netDirectSocket);
    netDirectSocket = 0;
}

extern unsigned __int64 netEnteredLobbyId;
namespace gf { bool isInitialized(); }
extern void* GetGalaxyNetworking();

// FUNCTION: SWBF2 0x00617e50
int NetSocket::SendSocket(int iDest, char* pData, int iLen)
{
    unsigned __int64* pPeer = NetCommP::GetAddr(iDest);
    if (NetComm::IsLAN())
    {
        int n = iDest == 68
              ? send((SOCKET)netBroadcastSendSocket, pData, iLen, 0)
              : sendto((SOCKET)netDirectSocket, pData, iLen, 0,
                       (const sockaddr*)(pPeer + 1), sizeof(sockaddr_in));
        if (n == SOCKET_ERROR)
        {
            DecodeError(WSAGetLastError());
        }
        return n;
    }
    if (!gf::isInitialized())
    {
        return 0;
    }
    if (iDest > 64 || !*(unsigned __int64*)(&netEnteredLobbyId))
    {
        return -1;
    }

    void* pNetworking = GetGalaxyNetworking();
    return ((galaxy::api::INetworking*)pNetworking)->SendP2PPacket(
               galaxy::api::GalaxyID(*pPeer), pData, iLen, 2, 0)
         ? iLen : -1;
}

// FUNCTION: SWBF2 0x00617f40
int NetSocket::SendVoiceSocket(int iDest, char* pData, int iLen, int iChannel)
{
    return SendSocket(iDest, pData, iLen);
}

extern bool netIsHost;
extern int netHostIndex;
extern void __cdecl JournalReceiveTraceStub();
typedef void (__cdecl *ReceiveTraceFn)(const void*, int);

// GLOBAL: SWBF2 0x01fab0d0
unsigned char netGalaxyReceiveBuffer[2048];

// GLOBAL: SWBF2 0x01fab8e0
unsigned char netLanReceiveBuffer[2048];

static void ProcessReceivedPacket(const unsigned char* pData, unsigned int size,
                                   unsigned int prefix, bool classifyByType,
                                   const NetSocketPeerRecord& peer)
{
    NetPacket* pPacket = NetCommP::MakeReceivedPacket(pData, size, prefix, classifyByType);
    if (pPacket)
    {
        NetCommP::RecvPacket(pPacket, &peer);
    }
}

struct ReceivePacketPrefix
{
    unsigned int value;
    ReceivePacketPrefix() : value(3) {}
};

// FUNCTION: SWBF2 0x00617f50
void NetSocket::RecvSockets()
{

    // GLOBAL: SWBF2 0x01fab8dc
    static ReceivePacketPrefix packetPrefix;

    sockaddr_in from;
    int fromLength;
    int remaining = 80;
    int received = SOCKET_ERROR;
    do
    {
        fromLength = sizeof(from);
        --remaining;
        received = recvfrom((SOCKET)netDirectSocket, (char*)netLanReceiveBuffer,
                            sizeof(netLanReceiveBuffer), 0, (sockaddr*)&from, &fromLength);
        if (received == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                DecodeError(error);
            }
            break;
        }
        ((ReceiveTraceFn)JournalReceiveTraceStub)(netLanReceiveBuffer, received);
        ((ReceiveTraceFn)JournalReceiveTraceStub)(&from, sizeof(from));
        NetSocketPeerRecord peer = { 0 };
        memcpy(peer.mSocketAddress, &from, sizeof(from));
        ProcessReceivedPacket(netLanReceiveBuffer, received, packetPrefix.value, true, peer);
    }
    while (remaining);
    if (!remaining)
    {
        do
        {
            fromLength = sizeof(from);
            received = recvfrom((SOCKET)netDirectSocket, (char*)netLanReceiveBuffer,
                                sizeof(netLanReceiveBuffer), 0, (sockaddr*)&from, &fromLength);
            if (received != SOCKET_ERROR)
            {
                ((ReceiveTraceFn)JournalReceiveTraceStub)(netLanReceiveBuffer, 1);
                ((ReceiveTraceFn)JournalReceiveTraceStub)(&from, sizeof(from));
            }
        }
        while (received != SOCKET_ERROR);
    }

    if (netBroadcastSendSocket)
    {
        remaining = 64;
        do
        {
            fromLength = sizeof(from);
            --remaining;
            received = recvfrom((SOCKET)netBroadcastRecvSocket, (char*)netLanReceiveBuffer,
                                1012 - packetPrefix.value, 0, (sockaddr*)&from, &fromLength);
            if (received == SOCKET_ERROR)
            {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK)
                {
                    DecodeError(error);
                }
                break;
            }
            ((ReceiveTraceFn)JournalReceiveTraceStub)(netLanReceiveBuffer, received);
            ((ReceiveTraceFn)JournalReceiveTraceStub)(&from, sizeof(from));
            NetSocketPeerRecord peer = { 0 };
            memcpy(peer.mSocketAddress, &from, sizeof(from));
            ProcessReceivedPacket(netLanReceiveBuffer, received, packetPrefix.value, false, peer);
        }
        while (remaining);
        if (!remaining)
        {
            do
            {
                fromLength = sizeof(from);
                received = recvfrom((SOCKET)netBroadcastRecvSocket, (char*)netLanReceiveBuffer,
                                    sizeof(netLanReceiveBuffer), 0, (sockaddr*)&from, &fromLength);
            }
            while (received != SOCKET_ERROR);
        }
    }
}

extern void* GetGalaxyNetworking();
extern void* GetGalaxyHostNetworking();

static bool PollGalaxy(void* pNetworking, unsigned int& size)
{
    return ((galaxy::api::INetworking*)pNetworking)->IsP2PPacketAvailable(&size, 0);
}

static bool ReadGalaxy(void* pNetworking, unsigned int capacity,
                       unsigned int& size, NetSocketPeerRecord& peer)
{
    return ((galaxy::api::INetworking*)pNetworking)->ReadP2PPacket(
        netGalaxyReceiveBuffer, capacity, &size, *(galaxy::api::GalaxyID*)&peer.mGalaxyId, 0);
}

static void DiscardGalaxy(void* pNetworking)
{
    ((galaxy::api::INetworking*)pNetworking)->PopP2PPacket(0);
}

// FUNCTION: SWBF2 0x00618250
void NetSocket::RecvGalaxySockets()
{

    // GLOBAL: SWBF2 0x01fab8d0
    static ReceivePacketPrefix packetPrefix;

    unsigned int size = 0;
    for (int count = 0; ; )
    {
        void* pNetworking = GetGalaxyNetworking();
        if (!PollGalaxy(pNetworking, size))
        {
            break;
        }
        if (++count >= 80 || size > 2048)
        {
            DiscardGalaxy(GetGalaxyNetworking());
            continue;
        }
        NetSocketPeerRecord peer = { 0 };
        if (ReadGalaxy(GetGalaxyNetworking(), 2048, size, peer))
        {
            ((ReceiveTraceFn)JournalReceiveTraceStub)(netGalaxyReceiveBuffer, size);
            ((ReceiveTraceFn)JournalReceiveTraceStub)(&peer, sizeof(peer));
            ProcessReceivedPacket(netGalaxyReceiveBuffer, size, packetPrefix.value, true, peer);
        }
    }

    if (netIsHost || netHostIndex >= 0)
    {
        for (int count = 0; ; )
        {
            void* pNetworking = GetGalaxyHostNetworking();
            if (!PollGalaxy(pNetworking, size))
            {
                break;
            }
            if (++count >= 64 || size > 2048)
            {
                DiscardGalaxy(GetGalaxyHostNetworking());
                continue;
            }
            NetSocketPeerRecord peer = { 0 };
            if (ReadGalaxy(GetGalaxyHostNetworking(), 1012 - packetPrefix.value,
                           size, peer))
            {
                ((ReceiveTraceFn)JournalReceiveTraceStub)(netGalaxyReceiveBuffer, size);
                ((ReceiveTraceFn)JournalReceiveTraceStub)(&peer, sizeof(peer));
                ProcessReceivedPacket(netGalaxyReceiveBuffer, size, packetPrefix.value, false, peer);
            }
        }
    }
}
