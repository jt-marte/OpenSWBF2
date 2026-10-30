#include "RecentServers.h"

#include <string.h>

namespace RecentServers
{

static const char kDefaultPort[] = ":3658";

static void CopyName(const char* pcName, char* pcOut)
{
    // tabs and line breaks would split the file's lines; runs of them become one space
    int iLen = 0;
    bool bSpace = false;
    for (const char* pc = pcName ? pcName : ""; *pc && iLen < kNameMax - 1; ++pc)
    {
        const unsigned char c = (unsigned char)*pc;
        if (c < 0x20 || c == ' ')
        {
            bSpace = iLen > 0;
            continue;
        }
        if (bSpace)
        {
            pcOut[iLen++] = ' ';
            bSpace = false;
            if (iLen == kNameMax - 1)
            {
                break;
            }
        }
        pcOut[iLen++] = (char)c;
    }
    pcOut[iLen] = 0;
}

void Clear(List& list)
{
    memset(&list, 0, sizeof(list));
}

bool Normalize(const char* pcAddr, char* pcOut, int iOutSize)
{
    if (!pcAddr || !pcOut || iOutSize <= 0)
    {
        return false;
    }
    pcOut[0] = 0;
    while (*pcAddr == ' ')
    {
        ++pcAddr;
    }
    int iLen = (int)strlen(pcAddr);
    while (iLen > 0 && pcAddr[iLen - 1] == ' ')
    {
        --iLen;
    }
    if (iLen == 0 || iLen >= iOutSize || iLen >= kAddrMax)
    {
        return false;
    }
    for (int i = 0; i < iLen; ++i)
    {
        const unsigned char c = (unsigned char)pcAddr[i];
        if (c <= 0x20 || c == 0x7f)
        {
            return false;
        }
        pcOut[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    const int iPortLen = (int)sizeof(kDefaultPort) - 1;
    if (iLen > iPortLen && strncmp(pcOut + iLen - iPortLen, kDefaultPort, iPortLen) == 0)
    {
        iLen -= iPortLen;
    }
    pcOut[iLen] = 0;
    return pcOut[0] != ':';
}

bool Add(List& list, const char* pcAddr, const char* pcName)
{
    Entry entry;
    if (!Normalize(pcAddr, entry.acAddr, kAddrMax))
    {
        return false;
    }
    CopyName(pcName, entry.acName);

    int iFound = -1;
    for (int i = 0; i < list.miCount; ++i)
    {
        if (strcmp(list.maEntries[i].acAddr, entry.acAddr) == 0)
        {
            iFound = i;
            break;
        }
    }
    if (iFound >= 0 && entry.acName[0] == 0)
    {
        strcpy(entry.acName, list.maEntries[iFound].acName);
    }

    // shift the newer ones down over the old copy, or off the end
    int iLast = iFound >= 0 ? iFound : (list.miCount < kMaxServers ? list.miCount : kMaxServers - 1);
    for (int i = iLast; i > 0; --i)
    {
        list.maEntries[i] = list.maEntries[i - 1];
    }
    list.maEntries[0] = entry;
    if (iFound < 0 && list.miCount < kMaxServers)
    {
        ++list.miCount;
    }
    return true;
}

void Parse(const char* pcText, List& list)
{
    Clear(list);
    if (!pcText)
    {
        return;
    }
    const char* pcLine = pcText;
    while (*pcLine && list.miCount < kMaxServers)
    {
        const char* pcEnd = pcLine;
        while (*pcEnd && *pcEnd != '\n' && *pcEnd != '\r')
        {
            ++pcEnd;
        }
        const char* pcTab = pcLine;
        while (pcTab < pcEnd && *pcTab != '\t')
        {
            ++pcTab;
        }

        char acAddr[kAddrMax];
        char acName[kNameMax];
        const int iAddrLen = (int)(pcTab - pcLine);
        const int iNameLen = pcTab < pcEnd ? (int)(pcEnd - pcTab - 1) : 0;
        Entry entry;
        bool bOk = iAddrLen < kAddrMax;
        if (bOk)
        {
            memcpy(acAddr, pcLine, iAddrLen);
            acAddr[iAddrLen] = 0;
            const int iCopy = iNameLen < kNameMax - 1 ? iNameLen : kNameMax - 1;
            memcpy(acName, pcTab + (iNameLen > 0 ? 1 : 0), iCopy);
            acName[iCopy] = 0;
            bOk = Normalize(acAddr, entry.acAddr, kAddrMax);
        }
        for (int i = 0; bOk && i < list.miCount; ++i)
        {
            bOk = strcmp(list.maEntries[i].acAddr, entry.acAddr) != 0;
        }
        if (bOk)
        {
            CopyName(acName, entry.acName);
            list.maEntries[list.miCount++] = entry; // the file is already newest first
        }

        pcLine = pcEnd;
        while (*pcLine == '\n' || *pcLine == '\r')
        {
            ++pcLine;
        }
    }
}

int Serialize(const List& list, char* pcOut, int iOutSize)
{
    if (!pcOut || iOutSize <= 0)
    {
        return -1;
    }
    int iLen = 0;
    pcOut[0] = 0;
    for (int i = 0; i < list.miCount && i < kMaxServers; ++i)
    {
        const Entry& entry = list.maEntries[i];
        const int iAddr = (int)strlen(entry.acAddr);
        const int iName = (int)strlen(entry.acName);
        if (iLen + iAddr + 1 + iName + 2 >= iOutSize)
        {
            pcOut[0] = 0;
            return -1;
        }
        memcpy(pcOut + iLen, entry.acAddr, iAddr);
        iLen += iAddr;
        pcOut[iLen++] = '\t';
        memcpy(pcOut + iLen, entry.acName, iName);
        iLen += iName;
        pcOut[iLen++] = '\r';
        pcOut[iLen++] = '\n';
    }
    pcOut[iLen] = 0;
    return iLen;
}

}
