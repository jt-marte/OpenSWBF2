#pragma once

#include <string.h>

namespace SimDigestCore
{

const unsigned int kOffsetBasis = 0x811C9DC5u;
const unsigned int kPrime = 0x01000193u;

inline unsigned int AddBytes(unsigned int uHash, const void* pData, unsigned int uSize)
{
    const unsigned char* pucBytes = (const unsigned char*)pData;
    for (unsigned int i = 0; i < uSize; ++i)
    {
        uHash = (uHash ^ pucBytes[i]) * kPrime;
    }
    return uHash;
}

inline unsigned int AddU32(unsigned int uHash, unsigned int uValue)
{
    unsigned char aucBytes[4];
    aucBytes[0] = (unsigned char)(uValue & 0xFF);
    aucBytes[1] = (unsigned char)((uValue >> 8) & 0xFF);
    aucBytes[2] = (unsigned char)((uValue >> 16) & 0xFF);
    aucBytes[3] = (unsigned char)((uValue >> 24) & 0xFF);
    return AddBytes(uHash, aucBytes, 4);
}

inline unsigned int AddInt(unsigned int uHash, int iValue)
{
    unsigned int uBits;
    memcpy(&uBits, &iValue, 4);
    return AddU32(uHash, uBits);
}

inline unsigned int AddFloat(unsigned int uHash, float fValue)
{
    unsigned int uBits;
    memcpy(&uBits, &fValue, 4);
    return AddU32(uHash, uBits);
}

}
