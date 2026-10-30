#include "third_party/doctest/doctest.h"

#include <random>

#include "testutil.h"
#include "nettest.h"
#include "corpus_replay.h"

static const size_t MAX_DATAGRAM = 1003;

static std::vector<ByteVec> LoadBases()
{
    std::vector<ByteVec> bases;
    const char* apcReaders[2] = {"recvgroup", "recvpacket"};
    for (int r = 0; r < 2; ++r)
    {
        std::vector<std::string> files = ListCorpus(apcReaders[r], false);
        for (size_t i = 0; i < files.size(); ++i)
        {
            ByteVec bytes;
            if (LoadCorpusFile(files[i], bytes))
            {
                bases.push_back(bytes);
            }
        }
    }
    return bases;
}

static ByteVec MakeInput(unsigned int uSeed, unsigned int uIter, const std::vector<ByteVec>& bases)
{
    std::mt19937 rng(uSeed ^ (uIter * 0x9E3779B1u));
    ByteVec b = bases[rng() % bases.size()];
    static const unsigned char acInteresting[5] = {0, 1, 0x7F, 0x80, 0xFF};
    int iMutations = 1 + (int)(rng() % 4);
    for (int m = 0; m < iMutations; ++m)
    {
        switch (rng() % 6)
        {
        case 0:
            if (!b.empty())
            {
                size_t uPos = rng() % b.size();
                unsigned int uBit = rng() % 8;
                b[uPos] ^= (unsigned char)(1u << uBit);
            }
            break;
        case 1:
            if (!b.empty())
            {
                unsigned int uVal = rng() % 5;
                size_t uPos = rng() % b.size();
                b[uPos] = acInteresting[uVal];
            }
            break;
        case 2:
        {
            unsigned char uByte = (unsigned char)rng();
            size_t uPos = rng() % (b.size() + 1);
            b.insert(b.begin() + uPos, uByte);
            break;
        }
        case 3:
            if (!b.empty())
            {
                b.erase(b.begin() + rng() % b.size());
            }
            break;
        case 4:
            b.resize(rng() % (b.size() + 1));
            break;
        default:
        {
            const ByteVec& other = bases[rng() % bases.size()];
            size_t uCut = rng() % (b.size() + 1);
            size_t uFrom = rng() % (other.size() + 1);
            b.resize(uCut);
            b.insert(b.end(), other.begin() + uFrom, other.end());
            break;
        }
        }
        if (b.size() > MAX_DATAGRAM)
        {
            b.resize(MAX_DATAGRAM);
        }
    }

    while (b.size() < 5)
    {
        b.push_back((unsigned char)rng());
    }
    unsigned int uCount = 1 + rng() % 4;
    b[0] = (rng() & 1) ? (unsigned char)(11 + rng() % 5) : (unsigned char)(rng() % 30);
    b[3] = (unsigned char)(rng() % uCount);
    b[4] = (unsigned char)(uCount | ((rng() & 1) ? 0x80 : 0));
    return b;
}

static unsigned int Fnv(unsigned int uSum, const ByteVec& b)
{
    for (size_t i = 0; i < b.size(); ++i)
    {
        uSum = (uSum ^ b[i]) * 16777619u;
    }
    return (uSum ^ (unsigned int)b.size()) * 16777619u;
}

static bool RunFuzz(unsigned int uSeed, unsigned int uCount, unsigned int* puSum = 0)
{
    std::vector<ByteVec> bases = LoadBases();
    REQUIRE(!bases.empty());
    unsigned int uSum = 2166136261u;
    for (unsigned int uIter = 0; uIter < uCount; ++uIter)
    {
        char acLabel[64];
        sprintf(acLabel, "seed=%u iter=%u", uSeed, uIter);
        WatchdogArm(acLabel);
        ByteVec in = MakeInput(uSeed, uIter, bases);
        uSum = Fnv(uSum, in);
        if (!ReplayRecvPacketBytes(&in[0], (unsigned int)in.size()))
        {
            printf("fuzz FAIL pool not whole: %s\n", acLabel);
            fflush(stdout);
            return false;
        }
    }
    printf("fuzz-checksum seed=%u count=%u sum=0x%08x\n", uSeed, uCount, uSum);
    fflush(stdout);
    if (puSum)
    {
        *puSum = uSum;
    }
    return true;
}

TEST_CASE("fuzz default seeds" * doctest::test_suite("fuzz"))
{
    const unsigned int auSeeds[4] = {1, 2, 3, 0xC0FFEE};
    const unsigned int auSums[4] = {0xac892f84, 0x10ce3326, 0xec1f98d7, 0xd649cad9};
    for (int i = 0; i < 4; ++i)
    {
        unsigned int uSum = 0;
        CHECK(RunFuzz(auSeeds[i], 2000, &uSum));
        CHECK(uSum == auSums[i]);
    }
}

TEST_CASE("fuzz stress soak" * doctest::test_suite("stress"))
{
    if (gFuzzDump)
    {
        std::vector<ByteVec> bases = LoadBases();
        REQUIRE(!bases.empty());
        REQUIRE(gFuzzOutPath[0] != 0);
        ByteVec in = MakeInput(gFuzzSeed, gFuzzIter, bases);
        FILE* pFile = fopen(gFuzzOutPath, "wb");
        REQUIRE(pFile);
        fwrite(&in[0], 1, in.size(), pFile);
        fclose(pFile);
        return;
    }
    CHECK(RunFuzz(gFuzzSeed, gFuzzCount ? gFuzzCount : 100000));
}

struct WireSeed
{
    const char* pcReader;
    ByteVec bytes;
};

static std::vector<WireSeed> LoadWireBases()
{
    std::vector<WireSeed> bases;
    const char* apcReaders[6] = {"chat-client", "chat-server", "upload", "rcon", "load", "turnrate"};
    for (int r = 0; r < 6; ++r)
    {
        std::vector<std::string> files = ListCorpus(apcReaders[r], false);
        for (size_t i = 0; i < files.size(); ++i)
        {
            WireSeed seed;
            seed.pcReader = apcReaders[r];
            if (LoadCorpusFile(files[i], seed.bytes))
            {
                bases.push_back(seed);
            }
        }
    }
    return bases;
}

static WireSeed MakeWireInput(unsigned int uSeed, unsigned int uIter, const std::vector<WireSeed>& bases)
{
    std::mt19937 rng(uSeed ^ (uIter * 0x9E3779B1u));
    WireSeed seed = bases[rng() % bases.size()];
    ByteVec& b = seed.bytes;
    int iMutations = 1 + (int)(rng() % 4);
    for (int m = 0; m < iMutations; ++m)
    {
        switch (rng() % 4)
        {
        case 0:
            if (!b.empty())
            {
                size_t uPos = rng() % b.size();
                unsigned int uBit = rng() % 8;
                b[uPos] ^= (unsigned char)(1u << uBit);
            }
            break;
        case 1:
            if (!b.empty())
            {
                unsigned char uVal = (unsigned char)rng();
                size_t uPos = rng() % b.size();
                b[uPos] = uVal;
            }
            break;
        case 2:
            b.resize(rng() % (b.size() + 1));
            break;
        default:
        {
            size_t uExtra = rng() % 200;
            for (size_t i = 0; i < uExtra && b.size() < 1500; ++i)
            {
                b.push_back((unsigned char)rng());
            }
            break;
        }
        }
    }
    return seed;
}

static bool RunWireFuzz(unsigned int uSeed, unsigned int uCount, unsigned int* puSum, unsigned int* puRejectSum)
{
    std::vector<WireSeed> bases = LoadWireBases();
    REQUIRE(!bases.empty());
    unsigned int uSum = 2166136261u;
    unsigned int uRejectSum = 2166136261u;
    for (unsigned int uIter = 0; uIter < uCount; ++uIter)
    {
        char acLabel[64];
        sprintf(acLabel, "wire seed=%u iter=%u", uSeed, uIter);
        WatchdogArm(acLabel);
        WireSeed in = MakeWireInput(uSeed, uIter, bases);
        uSum = Fnv(uSum, in.bytes);
        if (!ReplayNetWireBytes(in.pcReader, in.bytes.empty() ? 0 : &in.bytes[0], (unsigned int)in.bytes.size()))
        {
            printf("fuzz FAIL netwire reader=%s: %s\n", in.pcReader, acLabel);
            fflush(stdout);
            return false;
        }
        for (int i = 0; i < NET_REJECT_COUNT; ++i)
        {
            uRejectSum = (uRejectSum ^ (unsigned int)netRejectCounts[i]) * 16777619u;
        }
    }
    printf("fuzz-checksum netwire seed=%u count=%u sum=0x%08x rejects=0x%08x\n", uSeed, uCount, uSum,
           uRejectSum);
    fflush(stdout);
    *puSum = uSum;
    *puRejectSum = uRejectSum;
    return true;
}

TEST_CASE("fuzz netwire readers" * doctest::test_suite("fuzz"))
{
    const unsigned int auSeeds[4] = {1, 2, 3, 0xC0FFEE};
    const unsigned int auSums[4] = {0xd6d39bb0, 0xef022552, 0x2f8008ad, 0x61d4df43};
    // Hashes every reject counter, so adding a reason (guest_count, object_state) moves these pins.
    const unsigned int auRejectSums[4] = {0xf9bb5c00, 0x76dcf580, 0x0ac41008, 0x15ec8d20};
    for (int i = 0; i < 4; ++i)
    {
        unsigned int uSum = 0;
        unsigned int uRejectSum = 0;
        CHECK(RunWireFuzz(auSeeds[i], 2000, &uSum, &uRejectSum));
        CHECK(uSum == auSums[i]);
        CHECK(uRejectSum == auRejectSums[i]);
    }
}
