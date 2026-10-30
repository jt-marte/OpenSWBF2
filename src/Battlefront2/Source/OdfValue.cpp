#include <ctype.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "OdfValue.h"

#include "RedEngineFL/Util/RedLog.h"
#include "PebbleFL/Common/PblUnicode.h"

static bool IsDelimiter(char c)
{
    return isspace((unsigned char)c) || c == ',' || c == '"';
}

static void CopyToken(char* pcToken, const char* pcStart, const char* pcEnd)
{
    unsigned int uLen = (unsigned int)(pcEnd - pcStart);
    if (uLen > 64)
    {
        uLen = 64;
    }
    memcpy(pcToken, pcStart, uLen);
    pcToken[uLen] = '\0';
}

static int Scan(unsigned int uProperty, const char* pcValue, char* pcOut)
{
    int iCount = 0;
    const char* p = pcValue;
    while (*p)
    {
        if (IsDelimiter(*p))
        {
            ++p;
            continue;
        }
        const char* pcEnd = p;
        while (*pcEnd && !IsDelimiter(*pcEnd))
        {
            ++pcEnd;
        }
        const char* q = p;
        if (*q == '+' || *q == '-')
        {
            ++q;
        }
        char acToken[65];
        if (isalpha((unsigned char)*q))
        {
            char* pcStop = 0;
            double d = strtod(p, &pcStop);
            if (pcStop > p && !_finite(d) && pcStop == pcEnd)
            {
                CopyToken(acToken, p, pcEnd);
                if (pcOut)
                {
                    pcOut[p - pcValue] = '#';
                }
                LOG(LogLoad, Warning, "odf: property 0x%08X ignored non-finite value '%.64s'", uProperty,
                    acToken);
                ++iCount;
            }
        }
        else if (isdigit((unsigned char)*q) || (*q == '.' && isdigit((unsigned char)q[1])))
        {
            while (isdigit((unsigned char)*q))
            {
                ++q;
            }
            if (*q == '.')
            {
                ++q;
            }
            while (isdigit((unsigned char)*q))
            {
                ++q;
            }
            if (*q == 'e' || *q == 'E')
            {
                const char* r = q + 1;
                if (*r == '+' || *r == '-')
                {
                    ++r;
                }
                if (*r == '\0' || IsDelimiter(*r))
                {
                    CopyToken(acToken, p, pcEnd);
                    if (pcOut)
                    {
                        pcOut[q - pcValue] = '#';
                    }
                    LOG(LogLoad, Warning, "odf: property 0x%08X repaired dangling exponent '%.64s'",
                        uProperty, acToken);
                    ++iCount;
                }
            }
        }
        p = pcEnd;
    }
    return iCount;
}

int OdfValue::Sanitize(unsigned int uProperty, char* pcValue)
{
    return Scan(uProperty, pcValue, pcValue);
}

const char* OdfValue::SanitizeCopy(unsigned int uProperty, const char* pcValue, char* pcScratch,
                                   unsigned int uScratch)
{
    if (strlen(pcValue) < uScratch)
    {
        strlcpy(pcScratch, pcValue, uScratch);
        return Scan(uProperty, pcScratch, pcScratch) ? pcScratch : pcValue;
    }
    return Scan(uProperty, pcValue, 0) ? "#" : pcValue;
}
