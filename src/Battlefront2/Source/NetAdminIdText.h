#pragma once

#include <stdio.h>

inline int FormatAdminIdText(char* pcBuf, unsigned __int64 uAddr)
{
    return _snprintf(pcBuf, 32, "%ull", (unsigned int)uAddr);
}
