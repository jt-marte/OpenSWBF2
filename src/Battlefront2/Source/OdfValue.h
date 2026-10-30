#pragma once

namespace OdfValue
{
    int Sanitize(unsigned int uProperty, char* pcValue);
    const char* SanitizeCopy(unsigned int uProperty, const char* pcValue, char* pcScratch,
                             unsigned int uScratch);
}
