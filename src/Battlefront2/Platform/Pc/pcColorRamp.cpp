#include <math.h>
#include <cmath>

static_assert(sizeof(sin(1.0f)) == 4, "float math overloads");

#include "Battlefront2/Platform/Pc/pcColorRamp.h"

void ColorRamp::Build(const float afLevel[3], unsigned short awOut[256])
{
    const float fScale = afLevel[0] * 1.8f + 0.1f;
    const float fOffset = afLevel[1] * 1.5f - 0.75f;
    const float fExponent = afLevel[2] * 1.8f + 0.1f;

    for (int i = 0; i < 256; i++)
    {
        float fValue = pow(((float)i * (1.0f / 255.0f) - 0.5f + fOffset) * fScale + 0.5f,
                           fExponent);
        if (fValue < 0.0f)
        {
            fValue = 0.0f;
        }
        else if (fValue > 1.0f)
        {
            fValue = 1.0f;
        }

        awOut[i] = (unsigned short)(int)(fValue * 65535.0f);
    }
}
