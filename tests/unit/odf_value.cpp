#include "third_party/doctest/doctest.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testutil.h"
#include "Battlefront2/Source/OdfValue.h"

namespace
{
    const unsigned int kProperty = 0x1234ABCDu;
    const unsigned int kSentinel = 0x7FA5A5A5u;

    unsigned int ParseBits(const char* pcValue, int& iRet)
    {
        float f;
        unsigned int uBits = kSentinel;
        memcpy(&f, &uBits, sizeof(f));
        iRet = sscanf(pcValue, "%f", &f);
        memcpy(&uBits, &f, sizeof(uBits));
        return uBits;
    }

    void CheckRejected(const char* pcToken)
    {
        INFO(pcToken);
        char ac[64];
        strcpy(ac, pcToken);
        LogCapture capture;
        CHECK(OdfValue::Sanitize(kProperty, ac) == 1);
        CHECK(strlen(ac) == strlen(pcToken));
        int iRet = -2;
        unsigned int uBits = ParseBits(ac, iRet);
        CHECK(iRet == 0);
        CHECK(uBits == kSentinel);
        CHECK(atof(ac) == 0.0);
        CHECK(capture.Count("odf: property 0x") == 1);
    }

    void CheckUnchanged(const char* pcValue)
    {
        INFO(pcValue);
        char ac[64];
        strcpy(ac, pcValue);
        LogCapture capture;
        CHECK(OdfValue::Sanitize(kProperty, ac) == 0);
        CHECK(memcmp(ac, pcValue, strlen(pcValue) + 1) == 0);
        CHECK(capture.Count("odf: property") == 0);
    }
}

TEST_CASE("odf value guard rejects non-finite tokens" * doctest::test_suite("unit"))
{
    static const char* const kTokens[] = {
        "nan", "NAN", "NaN", "+nan", "-nan", "nan()", "nan(ind)", "-nan(ind)", "nan(snan)",
        "inf", "INF", "+inf", "-inf", "infinity", "INFINITY", "Infinity", "-INFINITY", " nan"};
    for (size_t i = 0; i < sizeof(kTokens) / sizeof(kTokens[0]); ++i)
    {
        CheckRejected(kTokens[i]);
    }

    char ac[64];
    strcpy(ac, "1.0 nan 2.0");
    LogCapture capture;
    CHECK(OdfValue::Sanitize(kProperty, ac) == 1);
    CHECK(strlen(ac) == strlen("1.0 nan 2.0"));
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    CHECK(sscanf(ac, "%f %f %f", &a, &b, &c) == 1);
    CHECK(a == 1.0f);
    CHECK(capture.Count("odf: property 0x") == 1);
}

TEST_CASE("odf value guard repairs dangling exponents" * doctest::test_suite("unit"))
{
    static const char* const kTokens[] = {"1e", "1e+"};
    for (size_t i = 0; i < sizeof(kTokens) / sizeof(kTokens[0]); ++i)
    {
        INFO(kTokens[i]);
        char ac[64];
        strcpy(ac, kTokens[i]);
        LogCapture capture;
        CHECK(OdfValue::Sanitize(kProperty, ac) == 1);
        CHECK(strlen(ac) == strlen(kTokens[i]));
        int iRet = -2;
        unsigned int uBits = ParseBits(ac, iRet);
        CHECK(iRet == 1);
        CHECK(uBits == 0x3F800000u);
        CHECK(atof(ac) == 1.0);
        CHECK(capture.Count("odf: property 0x") == 1);
    }
}

TEST_CASE("odf value guard leaves finite and retail values alone" * doctest::test_suite("unit"))
{
    static const char* const kValues[] = {
        "1.0", "-2.5", "1e39", "1.#INF", "1.#QNAN", "0x10", "rep_inf_ep3_rifleman", ""};
    for (size_t i = 0; i < sizeof(kValues) / sizeof(kValues[0]); ++i)
    {
        CheckUnchanged(kValues[i]);
    }

    char acScratch[256];
    const char* pcClean = "1.0 2.0";
    {
        LogCapture capture;
        CHECK(OdfValue::SanitizeCopy(kProperty, pcClean, acScratch, sizeof(acScratch)) == pcClean);
        CHECK(capture.Count("odf: property") == 0);
    }
    {
        LogCapture capture;
        const char* pcOut = OdfValue::SanitizeCopy(kProperty, "inf", acScratch, sizeof(acScratch));
        CHECK(pcOut == acScratch);
        int iRet = -2;
        unsigned int uBits = ParseBits(pcOut, iRet);
        CHECK(iRet == 0);
        CHECK(uBits == kSentinel);
        CHECK(capture.Count("odf: property 0x") == 1);
    }
    {
        std::string sLong(296, '1');
        sLong += " inf";
        LogCapture capture;
        const char* pcOut = OdfValue::SanitizeCopy(kProperty, sLong.c_str(), acScratch, sizeof(acScratch));
        CHECK(strcmp(pcOut, "#") == 0);
        CHECK(sLong.size() == 300);
        CHECK(sLong.substr(296) == " inf");
        CHECK(capture.Count("odf: property 0x") == 1);
    }
}

TEST_CASE("odf value guard leaves string values whole" * doctest::test_suite("unit"))
{
    static const char* const kValues[] = {
        "info", "infantry_x.msh", "inf_trooper", "infantry", "nanny", "rep_inf_ep3_rifleman",
        "weapon inf_rifle", "info,nanny", "infx", "infinit", "nan("};
    for (size_t i = 0; i < sizeof(kValues) / sizeof(kValues[0]); ++i)
    {
        CheckUnchanged(kValues[i]);
    }
}
