#include "third_party/doctest/doctest.h"

#include <string>

#include <string.h>

#include "testutil.h"
#include "nettest.h"
#include "Battlefront2/Source/NetWire.h"

static const unsigned int kSentinel = 0x5a17c0deu;

static void FillPattern(unsigned char* pDst, unsigned int uLen)
{
    for (unsigned int i = 0; i < uLen; ++i)
    {
        pDst[i] = (unsigned char)(i * 7 + 1);
    }
}

static void BeginPayload(NetPktGroup& group, const unsigned char* pData, unsigned int uLen, bool bLengthBits)
{
    group.Reset();
    group.BeginWrite();
    if (bLengthBits)
    {
        group.WriteBits(uLen, 10);
    }
    group.WriteBytes((const char*)pData, (int)uLen);
    group.WriteBits(kSentinel, 32);
    group.EndWrite();
    group.BeginRead();
}

static bool SentinelDecodes(NetPktGroup& group)
{
    unsigned int uValue = 0;
    group.ReadBits(uValue, 32);
    return !group.IsBad() && uValue == kSentinel;
}

static bool CanaryIntact(const unsigned char* pBuf, unsigned int uFrom, unsigned int uTo)
{
    for (unsigned int i = uFrom; i < uTo; ++i)
    {
        if (pBuf[i] != 0xAA)
        {
            return false;
        }
    }
    return true;
}

TEST_CASE("chat payload over cap is truncated and the group stays in sync" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    unsigned char acSrc[600];
    FillPattern(acSrc, sizeof(acSrc));
    NetPktGroup group;
    BeginPayload(group, acSrc, 600, false);
    unsigned char acDst[292];
    memset(acDst, 0xAA, sizeof(acDst));
    CHECK(NetWire::ReadChatPayload(group, acDst, 276, 600));
    CHECK(memcmp(acDst, acSrc, 274) == 0);
    CHECK(acDst[274] == 0);
    CHECK(acDst[275] == 0);
    CHECK(CanaryIntact(acDst, 276, 292));
    CHECK(netRejectCounts[NET_REJECT_CHAT_OVERSIZE] == 1);
    CHECK(capture.Contains("net reject chat_oversize"));
    CHECK(SentinelDecodes(group));
    group.Free();
    CHECK(NetCountPackets(0) == 512);
}

TEST_CASE("chat payload at cap is copied whole without a reject" * doctest::test_suite("regression"))
{
    NetTestReset();
    unsigned char acSrc[276];
    FillPattern(acSrc, sizeof(acSrc));
    NetPktGroup group;
    BeginPayload(group, acSrc, 276, false);
    unsigned char acDst[292];
    memset(acDst, 0xAA, sizeof(acDst));
    CHECK(NetWire::ReadChatPayload(group, acDst, 276, 276));
    CHECK(memcmp(acDst, acSrc, 274) == 0);
    CHECK(acDst[275] == 0);
    CHECK(CanaryIntact(acDst, 276, 292));
    CHECK(netRejectCounts[NET_REJECT_CHAT_OVERSIZE] == 0);
    CHECK(SentinelDecodes(group));
    group.Free();
    CHECK(NetCountPackets(0) == 512);
}

TEST_CASE("chat payload past the group end marks the group bad" * doctest::test_suite("regression"))
{
    NetTestReset();
    unsigned char acSrc[8];
    FillPattern(acSrc, sizeof(acSrc));
    NetPktGroup group;
    BeginPayload(group, acSrc, 8, false);
    unsigned char acDst[292];
    memset(acDst, 0xAA, sizeof(acDst));
    CHECK(!NetWire::ReadChatPayload(group, acDst, 276, 5000));
    CHECK(group.IsBad());
    CHECK(CanaryIntact(acDst, 276, 292));
    group.Free();
    CHECK(NetCountPackets(0) == 512);
}

TEST_CASE("chat fields out of range are rejected" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    CHECK(NetWire::IsValidChatFields(63, 0, true));
    CHECK(NetWire::IsValidChatFields(64, 0, false));
    CHECK(NetWire::IsValidChatFields(0, -1, true));
    CHECK(NetWire::IsValidChatFields(0, 7, true));
    CHECK(netRejectCounts[NET_REJECT_CHAT_FIELDS] == 0);
    CHECK(!NetWire::IsValidChatFields(64, 0, true));
    CHECK(capture.Contains("net reject chat_fields"));
    CHECK(!NetWire::IsValidChatFields(65, 0, false));
    CHECK(!NetWire::IsValidChatFields(-1, 0, false));
    CHECK(!NetWire::IsValidChatFields(0, 8, true));
    CHECK(!NetWire::IsValidChatFields(0, -2, false));
    CHECK(netRejectCounts[NET_REJECT_CHAT_FIELDS] == 5);
}

TEST_CASE("upload names outside the allowlist are unsafe" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    CHECK(NetWire::IsSafeUploadName("journal_1_2.dat"));
    CHECK(NetWire::IsSafeUploadName("values-3_4.dat"));
    CHECK(NetWire::IsSafeUploadName("abcdefghijklmno"));
    CHECK(NetWire::IsSafeUploadName("console.dat"));
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_NAME] == 0);
    const char* apcBad[] = {".hidden", "", "con", "CON.txt", "com1", "LPT9.x", "Nul", "a/b", "..", "a b",
                            "abcdefghijklmnop", "a\\b", "c:x"};
    int iBad = 0;
    for (unsigned int i = 0; i < sizeof(apcBad) / sizeof(apcBad[0]); ++i)
    {
        CHECK(!NetWire::IsSafeUploadName(apcBad[i]));
        ++iBad;
    }
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_NAME] == iBad);
    CHECK(capture.Contains("net reject upload_name"));
}

TEST_CASE("upload dirs outside the allowlist are unsafe" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    CHECK(NetWire::IsSafeUploadDir("Player_1"));
    CHECK(NetWire::IsSafeUploadDir("a-b"));
    CHECK(NetWire::IsSafeUploadDir("com0"));
    CHECK(NetWire::IsSafeUploadDir("data1"));
    CHECK(NetWire::IsSafeUploadDir("addons"));
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_NAME] == 0);
    const char* apcBad[] = {"", ".", "..", "con", "AUX", "nul", "prn", "com1", "LPT9", "a.b", "a\\b", "a b",
                            "data", "DATA", "Data", "addon", "AddOn"};
    int iBad = 0;
    for (unsigned int i = 0; i < sizeof(apcBad) / sizeof(apcBad[0]); ++i)
    {
        CHECK(!NetWire::IsSafeUploadDir(apcBad[i]));
        ++iBad;
    }
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_NAME] == iBad);
    CHECK(capture.Contains("net reject upload_name"));
}

TEST_CASE("upload chunk longer than the buffer is rejected and fully consumed" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    unsigned char acSrc[1023];
    FillPattern(acSrc, sizeof(acSrc));
    NetPktGroup group;
    BeginPayload(group, acSrc, 1023, true);
    unsigned char acDst[1028];
    memset(acDst, 0xAA, sizeof(acDst));
    unsigned int uLength = 0;
    CHECK(!NetWire::ReadUploadChunk(group, (char*)acDst, 1012, 5000, uLength));
    CHECK(uLength == 1023);
    CHECK(CanaryIntact(acDst, 1012, 1028));
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_LENGTH] == 1);
    CHECK(capture.Contains("net reject upload_length"));
    CHECK(SentinelDecodes(group));
    group.Free();
    CHECK(NetCountPackets(0) == 512);
}

TEST_CASE("upload chunk longer than the remaining size is rejected" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    unsigned char acSrc[100];
    FillPattern(acSrc, sizeof(acSrc));
    NetPktGroup group;
    BeginPayload(group, acSrc, 100, true);
    unsigned char acDst[1028];
    memset(acDst, 0xAA, sizeof(acDst));
    unsigned int uLength = 0;
    CHECK(!NetWire::ReadUploadChunk(group, (char*)acDst, 1012, 50, uLength));
    CHECK(uLength == 100);
    CHECK(CanaryIntact(acDst, 1012, 1028));
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_UNDERFLOW] == 1);
    CHECK(capture.Contains("net reject upload_underflow"));
    CHECK(SentinelDecodes(group));
    group.Free();
    CHECK(NetCountPackets(0) == 512);
}

TEST_CASE("upload chunk within limits is copied" * doctest::test_suite("regression"))
{
    NetTestReset();
    unsigned char acSrc[1012];
    FillPattern(acSrc, sizeof(acSrc));
    NetPktGroup group;
    BeginPayload(group, acSrc, 1012, true);
    unsigned char acDst[1028];
    memset(acDst, 0xAA, sizeof(acDst));
    unsigned int uLength = 0;
    CHECK(NetWire::ReadUploadChunk(group, (char*)acDst, 1012, 1012, uLength));
    CHECK(uLength == 1012);
    CHECK(memcmp(acDst, acSrc, 1012) == 0);
    CHECK(CanaryIntact(acDst, 1012, 1028));
    CHECK(SentinelDecodes(group));
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_LENGTH] == 0);
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_UNDERFLOW] == 0);
    group.Free();
    CHECK(NetCountPackets(0) == 512);
}

TEST_CASE("upload discard consumes the shape the header bit names" * doctest::test_suite("regression"))
{
    NetTestReset();
    unsigned char acSrc[300];
    FillPattern(acSrc, sizeof(acSrc));
    for (int iShape = 0; iShape < 3; ++iShape)
    {
        NetPktGroup group;
        group.Reset();
        group.BeginWrite();
        if (iShape == 0)
        {
            NetWire::WriteUploadChunk(group, 5, (const char*)acSrc, 300);
        }
        else
        {
            NetWire::WriteUploadHeader(group, 5, iShape == 1, "journal_1_2.dat", 5000);
        }
        group.WriteBits(kSentinel, 32);
        group.EndWrite();
        group.BeginRead();
        bool bHeader = false;
        unsigned int uSeq = 0;
        group.Read(bHeader);
        group.ReadBits(uSeq, 32);
        CHECK(bHeader == (iShape != 0));
        CHECK(uSeq == 5);
        NetWire::DiscardUpload(group, bHeader);
        CHECK(SentinelDecodes(group));
        group.Free();
    }
    CHECK(netRejectCounts[NET_REJECT_UPLOAD_LENGTH] == 0);
    CHECK(NetCountPackets(0) == 512);
}

TEST_CASE("rcon line is capped to the bytes left in the packet" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    char acData[200];
    memset(acData, 'x', sizeof(acData));
    unsigned short ausOut[72];
    memset(ausOut, 0xAA, sizeof(ausOut));
    CHECK(NetWire::ReadRconLine(acData, 10, 200, ausOut) == 10);
    CHECK(ausOut[0] == 'x');
    CHECK(ausOut[9] == 'x');
    CHECK(ausOut[10] == 0);
    CHECK(ausOut[11] == 0xAAAA);
    CHECK(netRejectCounts[NET_REJECT_RCON_LINE] == 1);
    CHECK(capture.Contains("net reject rcon_line"));
}

TEST_CASE("rcon line is capped at 63 characters" * doctest::test_suite("regression"))
{
    NetTestReset();
    char acData[200];
    memset(acData, 'y', sizeof(acData));
    unsigned short ausOut[72];
    memset(ausOut, 0xAA, sizeof(ausOut));
    CHECK(NetWire::ReadRconLine(acData, 100, 63, ausOut) == 63);
    CHECK(ausOut[62] == 'y');
    CHECK(ausOut[63] == 0);
    CHECK(ausOut[64] == 0xAAAA);
    CHECK(netRejectCounts[NET_REJECT_RCON_LINE] == 0);
    memset(ausOut, 0xAA, sizeof(ausOut));
    CHECK(NetWire::ReadRconLine(acData, 100, 200, ausOut) == 63);
    CHECK(ausOut[63] == 0);
    CHECK(ausOut[64] == 0xAAAA);
    CHECK(netRejectCounts[NET_REJECT_RCON_LINE] == 1);
    memset(ausOut, 0xAA, sizeof(ausOut));
    CHECK(NetWire::ReadRconLine(acData, -5, 200, ausOut) == 0);
    CHECK(ausOut[0] == 0);
    CHECK(ausOut[1] == 0xAAAA);
}

TEST_CASE("rcon line keeps the sign extension of high bit characters" * doctest::test_suite("regression"))
{
    NetTestReset();
    char acData[4] = {(char)0x80, 'a', 0, 0};
    unsigned short ausOut[72];
    CHECK(NetWire::ReadRconLine(acData, 4, 2, ausOut) == 2);
    CHECK(ausOut[0] == 0xFF80);
    CHECK(ausOut[1] == 'a');
    CHECK(ausOut[2] == 0);
}

TEST_CASE("rcon auth needs eight bytes for J and paren but seven for s" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    char acS[8] = {0, 0, 0, 0, 0, 's', 0, 0};
    CHECK(NetWire::IsRconAuthorized(acS, 7, 'k', 'm'));
    acS[6] = 1;
    CHECK(!NetWire::IsRconAuthorized(acS, 7, 'k', 'm'));
    CHECK(netRejectCounts[NET_REJECT_RCON_AUTH_SHORT] == 0);
    char acJ[8] = {0, 0, 0, 0, 0, 'J', 0, 'k'};
    CHECK(!NetWire::IsRconAuthorized(acJ, 7, 'k', 'm'));
    CHECK(netRejectCounts[NET_REJECT_RCON_AUTH_SHORT] == 1);
    CHECK(capture.Contains("net reject rcon_auth_short"));
    acJ[5] = ')';
    CHECK(!NetWire::IsRconAuthorized(acJ, 7, 'k', 'm'));
    CHECK(netRejectCounts[NET_REJECT_RCON_AUTH_SHORT] == 2);
    CHECK(!NetWire::IsRconAuthorized(acJ, 6, 'k', 'm'));
    CHECK(netRejectCounts[NET_REJECT_RCON_AUTH_SHORT] == 2);
}

TEST_CASE("rcon auth accepts a matching key and refuses others" * doctest::test_suite("regression"))
{
    NetTestReset();
    char acJ[8] = {0, 0, 0, 0, 0, 'J', 0, 'k'};
    CHECK(NetWire::IsRconAuthorized(acJ, 8, 'k', 'm'));
    CHECK(NetWire::IsRconAuthorized(acJ, 8, 'z', 'k'));
    CHECK(!NetWire::IsRconAuthorized(acJ, 8, 'a', 'b'));
    acJ[6] = 1;
    CHECK(!NetWire::IsRconAuthorized(acJ, 8, 'k', 'm'));
    acJ[6] = 0;
    acJ[5] = 'q';
    CHECK(!NetWire::IsRconAuthorized(acJ, 8, 'k', 'm'));
}

TEST_CASE("login limiter allows five attempts per ten seconds" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    NetWire::LoginLimiter limiter;
    for (int i = 0; i < 5; ++i)
    {
        CHECK(limiter.Allow(0x0a000001u, 1.0f + (float)i * 0.1f));
    }
    CHECK(!limiter.Allow(0x0a000001u, 2.0f));
    CHECK(netRejectCounts[NET_REJECT_RCON_LOGIN_RATE] == 1);
    CHECK(capture.Contains("net reject rcon_login_rate"));
    CHECK(limiter.Allow(0x0a000002u, 2.0f));
    CHECK(!limiter.Allow(0x0a000001u, 10.9f));
    CHECK(limiter.Allow(0x0a000001u, 11.0f));
    CHECK(limiter.Allow(0, 11.0f));
}

TEST_CASE("login limiter resets the window when the clock goes backwards" * doctest::test_suite("regression"))
{
    NetTestReset();
    NetWire::LoginLimiter limiter;
    for (int i = 0; i < 5; ++i)
    {
        CHECK(limiter.Allow(0x0a000001u, 3600.0f));
    }
    CHECK(!limiter.Allow(0x0a000001u, 3601.0f));
    CHECK(limiter.Allow(0x0a000001u, 0.5f));
    for (int i = 0; i < 4; ++i)
    {
        CHECK(limiter.Allow(0x0a000001u, 1.0f));
    }
    CHECK(!limiter.Allow(0x0a000001u, 1.5f));
}

TEST_CASE("login limiter key zero is never limited" * doctest::test_suite("regression"))
{
    NetTestReset();
    NetWire::LoginLimiter limiter;
    for (int i = 0; i < 100; ++i)
    {
        CHECK(limiter.Allow(0, 1.0f));
    }
    CHECK(netRejectCounts[NET_REJECT_RCON_LOGIN_RATE] == 0);
}

TEST_CASE("login limiter evicts the oldest entry when the table is full" * doctest::test_suite("regression"))
{
    NetTestReset();
    NetWire::LoginLimiter limiter;
    for (int i = 0; i < 5; ++i)
    {
        CHECK(limiter.Allow(1, 0.0f));
    }
    CHECK(!limiter.Allow(1, 0.0f));
    for (unsigned int uKey = 2; uKey <= 16; ++uKey)
    {
        CHECK(limiter.Allow(uKey, 0.0f));
    }
    CHECK(limiter.Allow(17, 0.5f));
    CHECK(limiter.Allow(1, 1.0f));
}

TEST_CASE("player count is clamped to 1 through 64" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    CHECK(NetWire::ClampPlayerCount(1) == 1);
    CHECK(NetWire::ClampPlayerCount(64) == 64);
    CHECK(NetWire::ClampPlayerCount(32) == 32);
    CHECK(netRejectCounts[NET_REJECT_PLAYER_COUNT] == 0);
    CHECK(NetWire::ClampPlayerCount(0) == 1);
    CHECK(NetWire::ClampPlayerCount(127) == 64);
    CHECK(NetWire::ClampPlayerCount(65) == 64);
    CHECK(NetWire::ClampPlayerCount(0xffffffffu) == 64);
    CHECK(netRejectCounts[NET_REJECT_PLAYER_COUNT] == 4);
    CHECK(capture.Contains("net reject player_count"));
}

TEST_CASE("join guest count above one is rejected" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    CHECK(NetWire::IsValidGuestCount(0));
    CHECK(NetWire::IsValidGuestCount(1));
    CHECK(netRejectCounts[NET_REJECT_GUEST_COUNT] == 0);
    CHECK(!NetWire::IsValidGuestCount(2));
    CHECK(!NetWire::IsValidGuestCount(7));
    CHECK(netRejectCounts[NET_REJECT_GUEST_COUNT] == 2);
    CHECK(capture.Contains("net reject guest_count"));
}

TEST_CASE("lan player limit zero means a 64 player host" * doctest::test_suite("regression"))
{
    CHECK(NetWire::DecodeLanMaxPlayers(0) == 64);
    CHECK(NetWire::DecodeLanMaxPlayers(1) == 1);
    CHECK(NetWire::DecodeLanMaxPlayers(32) == 32);
    CHECK(NetWire::DecodeLanMaxPlayers(63) == 63);
}

static unsigned long Ip4(unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
    return (unsigned long)(a | (b << 8) | (c << 16) | (d << 24)); // network byte order in memory
}

TEST_CASE("lan transport accepts any unicast ipv4" * doctest::test_suite("regression"))
{
    CHECK(NetWire::IsUsableIPv4(Ip4(192, 168, 1, 20)));
    CHECK(NetWire::IsUsableIPv4(Ip4(10, 0, 0, 1)));
    CHECK(NetWire::IsUsableIPv4(Ip4(100, 73, 24, 63)));
    CHECK(NetWire::IsUsableIPv4(Ip4(25, 1, 2, 3)));
    CHECK(NetWire::IsUsableIPv4(Ip4(8, 8, 8, 8)));
    CHECK(NetWire::IsUsableIPv4(Ip4(127, 0, 0, 1)));
    CHECK(!NetWire::IsUsableIPv4(0));
    CHECK(!NetWire::IsUsableIPv4(0xffffffffu));
    CHECK(!NetWire::IsUsableIPv4(Ip4(0, 1, 2, 3)));
    CHECK(!NetWire::IsUsableIPv4(Ip4(224, 0, 0, 1)));
    CHECK(!NetWire::IsUsableIPv4(Ip4(239, 255, 255, 250)));
    CHECK(!NetWire::IsUsableIPv4(Ip4(240, 0, 0, 1)));
}

TEST_CASE("join ip text parses host and optional port" * doctest::test_suite("regression"))
{
    char acHost[32];
    int iPort = 0;
    CHECK(NetWire::ParseHostPort("192.168.1.20", acHost, sizeof(acHost), iPort, 3658));
    CHECK(std::string(acHost) == "192.168.1.20");
    CHECK(iPort == 3658);
    CHECK(NetWire::ParseHostPort("  10.0.0.5:3700 ", acHost, sizeof(acHost), iPort, 3658));
    CHECK(std::string(acHost) == "10.0.0.5");
    CHECK(iPort == 3700);
    CHECK(NetWire::ParseHostPort("my-server.example.com:65535", acHost, sizeof(acHost), iPort, 3658));
    CHECK(std::string(acHost) == "my-server.example.com");
    CHECK(iPort == 65535);
    CHECK(!NetWire::ParseHostPort("", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("   ", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort(":3658", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("1.2.3.4:", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("1.2.3.4:0", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("1.2.3.4:65536", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("1.2.3.4:123456", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("1.2.3.4:36x", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("1.2.3.4 5.6.7.8", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("::1", acHost, sizeof(acHost), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("host/name", acHost, sizeof(acHost), iPort, 3658));
    char acSmall[8];
    CHECK(NetWire::ParseHostPort("1.2.3.4", acSmall, sizeof(acSmall), iPort, 3658));
    CHECK(!NetWire::ParseHostPort("1.2.3.45", acSmall, sizeof(acSmall), iPort, 3658));
}

TEST_CASE("host bandwidth option goes past 6M" * doctest::test_suite("regression"))
{
    // Retail's nine options keep their slots, so old profiles keep their upload cap.
    const int aiRetail[9] = {3, 6, 9, 12, 19, 25, 38, 76, 153};
    for (unsigned int i = 0; i < 9; ++i)
    {
        CHECK(NetWire::HostBandwidthSlots(i) == aiRetail[i]);
    }
    CHECK(NetWire::kHostBandwidthOptions == 14);
    CHECK(NetWire::HostBandwidthSlots(9) * 40960 > 10000000);   // 10M
    CHECK(NetWire::HostBandwidthSlots(12) * 40960 > 100000000); // 100M
    // Unlimited still fits DoThrottle's int multiply by 40960.
    CHECK((long long)NetWire::HostBandwidthSlots(13) * 40960 < 2147483647LL);
    CHECK(NetWire::HostBandwidthSlots(13) * 40960 > 1000000000);
    CHECK(NetWire::HostBandwidthSlots(200) == NetWire::HostBandwidthSlots(13));
    CHECK(NetWire::kHostBandwidthDefault == 9);
}

TEST_CASE("hosting turn rate defaults to 60 and keeps 30 to 255" * doctest::test_suite("regression"))
{
    CHECK(NetWire::kDefaultTurnRate == 60);
    CHECK(NetWire::HostTurnRate(0) == 60);
    CHECK(NetWire::HostTurnRate(15) == 60); // retail profile values
    CHECK(NetWire::HostTurnRate(20) == 60);
    CHECK(NetWire::HostTurnRate(30) == 30);
    CHECK(NetWire::HostTurnRate(60) == 60);
    CHECK(NetWire::HostTurnRate(120) == 120);
    CHECK(NetWire::HostTurnRate(300) == 255);
}

TEST_CASE("turn rate outside the retail range is rejected" * doctest::test_suite("regression"))
{
    NetTestReset();
    LogCapture capture;
    CHECK(NetWire::IsValidTurnRate(20, 1));
    CHECK(NetWire::IsValidTurnRate(255, 1));
    CHECK(NetWire::IsValidTurnRate(255, 7));
    CHECK(NetWire::IsValidTurnRate(1, 1));
    CHECK(NetWire::IsValidTurnRate(7, 7));
    CHECK(netRejectCounts[NET_REJECT_TURN_RATE] == 0);
    CHECK(!NetWire::IsValidTurnRate(0, 1));
    CHECK(!NetWire::IsValidTurnRate(20, 0));
    CHECK(!NetWire::IsValidTurnRate(1024, 1));
    CHECK(!NetWire::IsValidTurnRate(3, 7));
    CHECK(!NetWire::IsValidTurnRate(-5, 1));
    CHECK(!NetWire::IsValidTurnRate(20, 8));
    CHECK(!NetWire::IsValidTurnRate(256, 1));
    CHECK(!NetWire::IsValidTurnRate(1023, 1));
    CHECK(!NetWire::IsValidTurnRate(1023, 7));
    CHECK(netRejectCounts[NET_REJECT_TURN_RATE] == 9);
    CHECK(capture.Contains("net reject turn_rate"));
}
