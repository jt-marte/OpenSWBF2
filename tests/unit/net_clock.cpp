#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string>

#include "Battlefront2/Source/NetClock.h"

// OpenSWBF2: at 120 turns a second a client's lead over the host crept from 13 to 41 turns in a
// minute on a quiet LAN, because the clock's numbers were retail's turn counts for 20 a second.
// They now scale with the rate, and stay retail's at 20.

namespace
{

std::string ReadSource(const char* pcPath)
{
    std::string sOut;
    FILE* pFile = fopen(pcPath, "rb");
    REQUIRE(pFile);
    int c;
    while ((c = fgetc(pFile)) != EOF)
    {
        if (c != '\r')
        {
            sOut += (char)c;
        }
    }
    fclose(pFile);
    return sOut;
}

bool Has(const std::string& sText, const char* pcNeedle)
{
    return sText.find(pcNeedle) != std::string::npos;
}

} // namespace

TEST_CASE("the clock keeps retail's numbers at 20 turns a second" * doctest::test_suite("unit"))
{
    CHECK(NetClock::StableWindow(0, 20) == 3);
    CHECK(NetClock::StableWindow(5, 20) == 2);
    CHECK(NetClock::StableWindow(10, 20) == 1);
    CHECK(NetClock::StableWindow(15, 20) == 0);
    CHECK(NetClock::LockedStep(20) == 1);
    CHECK(NetClock::MaxSubmitTurns(20) == 3);
    CHECK(NetClock::MaxSimTurns(20) == 4);
    CHECK(NetClock::JoinLead(20) == 4);
}

TEST_CASE("the clock's numbers keep their times at 60 and 120" * doctest::test_suite("unit"))
{
    // 150 ms window, 50 ms steps, 150 ms of turns a frame, 200 ms join lead
    CHECK(NetClock::StableWindow(0, 120) == 18);
    CHECK(NetClock::StableWindow(0, 60) == 9);
    CHECK(NetClock::LockedStep(120) == 6);
    CHECK(NetClock::LockedStep(60) == 3);
    CHECK(NetClock::MaxSubmitTurns(120) == 18);
    CHECK(NetClock::MaxSimTurns(120) == 24);
    CHECK(NetClock::JoinLead(120) == 24);
    // A settled window never shrinks below 25 ms, where whole turns are too fine to hit
    CHECK(NetClock::StableWindow(20, 120) == 3);
    CHECK(NetClock::StableWindow(20, 60) == 1);
    // Unknown rate: retail's
    CHECK(NetClock::JoinLead(0) == 4);
    CHECK(NetClock::LockedStep(30) == 1);
}

TEST_CASE("the lag byte saturates instead of wrapping" * doctest::test_suite("unit"))
{
    CHECK(NetClock::LagByte(0) == 128u);
    CHECK(NetClock::LagByte(-5) == 123u);
    CHECK(NetClock::LagByte(127) == 255u);
    CHECK(NetClock::LagByte(300) == 255u);
    CHECK(NetClock::LagByte(-128) == 0u);
    CHECK(NetClock::LagByte(-300) == 0u);
}

TEST_CASE("the client clock uses the scaled numbers" * doctest::test_suite("regression"))
{
    const std::string sNet = ReadSource("../src/Battlefront2/Source/NetGame.cpp");
    const std::string sSetup = ReadSource("../src/Battlefront2/Source/NetSetup.cpp");

    CHECK(Has(sNet, "const int iWindow = NetClock::StableWindow(timeStableCount, netTurnsPerSecond);"));
    CHECK(Has(sNet, "iLimit = NetClock::LockedStep(netTurnsPerSecond);"));
    CHECK(Has(sNet, "const int iMaxTurns = NetClock::MaxSubmitTurns(netTurnsPerSecond);"));
    CHECK(Has(sNet, "return NetClock::MaxSimTurns(netTurnsPerSecond);"));
    CHECK(Has(sNet, "group.WriteBits(NetClock::LagByte(GetLag(iPlayer)), 8);"));
    CHECK(Has(sNet, "group.WriteBits(NetClock::LagByte(GetLag(curDst)), 8);"));
    CHECK(Has(sSetup, "NetGameP::SetClientTurnNumber(turn + NetClock::JoinLead(netTurnsPerSecond));"));

    // The client's own inputs: 128 turns, every index through the mask
    CHECK(Has(sNet, "ProcessedInputs localClientTurns[kLocalTurnRing][2];"));
    CHECK_FALSE(Has(sNet, "& 0x1f]"));
    CHECK_FALSE(Has(sNet, "predictTurnNumber & 31"));
}
