#include "third_party/doctest/doctest.h"

#include <windows.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "testutil.h"

typedef unsigned __int64 u64;

enum MathKind
{
    MK_F1,
    MK_D1,
    MK_F2,
    MK_D2,
    MK_FI,
    MK_DI
};

struct UnaryFn
{
    const char* pcName;
    float (*pfnF)(float);
    double (*pfnD)(double);
    double dLo;
    double dHi;
};

struct BinaryFn
{
    const char* pcName;
    float (*pfnF)(float, float);
    double (*pfnD)(double, double);
    double dLo1;
    double dHi1;
    double dLo2;
    double dHi2;
};

static const double kPi = 3.14159265358979323846;

static const UnaryFn sUnary[] = {
    {"sin", [](float x) -> float { return sin(x); }, [](double x) -> double { return sin(x); }, -8 * kPi, 8 * kPi},
    {"cos", [](float x) -> float { return cos(x); }, [](double x) -> double { return cos(x); }, -8 * kPi, 8 * kPi},
    {"tan", [](float x) -> float { return tan(x); }, [](double x) -> double { return tan(x); }, -8 * kPi, 8 * kPi},
    {"asin", [](float x) -> float { return asin(x); }, [](double x) -> double { return asin(x); }, -1.0625, 1.0625},
    {"acos", [](float x) -> float { return acos(x); }, [](double x) -> double { return acos(x); }, -1.0625, 1.0625},
    {"atan", [](float x) -> float { return atan(x); }, [](double x) -> double { return atan(x); }, -8 * kPi, 8 * kPi},
    {"sqrt", [](float x) -> float { return sqrt(x); }, [](double x) -> double { return sqrt(x); }, 0.0, 1e4},
    {"exp", [](float x) -> float { return exp(x); }, [](double x) -> double { return exp(x); }, -20.0, 20.0},
    {"log", [](float x) -> float { return log(x); }, [](double x) -> double { return log(x); }, 0.0, 1e4},
    {"log10", [](float x) -> float { return log10(x); }, [](double x) -> double { return log10(x); }, 0.0, 1e4},
    {"floor", [](float x) -> float { return floor(x); }, [](double x) -> double { return floor(x); }, -1000.0, 1000.0},
    {"ceil", [](float x) -> float { return ceil(x); }, [](double x) -> double { return ceil(x); }, -1000.0, 1000.0},
};

static const BinaryFn sBinary[] = {
    {"atan2", [](float y, float x) -> float { return atan2(y, x); }, [](double y, double x) -> double { return atan2(y, x); },
     -8 * kPi, 8 * kPi, -8 * kPi, 8 * kPi},
    {"fmod", [](float x, float y) -> float { return fmod(x, y); }, [](double x, double y) -> double { return fmod(x, y); },
     -50.0, 50.0, -8.0, 8.0},
    {"pow", [](float x, float y) -> float { return pow(x, y); }, [](double x, double y) -> double { return pow(x, y); },
     0.0, 10.0, -4.0, 4.0},
};

static const int UNARY_COUNT = sizeof(sUnary) / sizeof(sUnary[0]);
static const int BINARY_COUNT = sizeof(sBinary) / sizeof(sBinary[0]);

struct MathForm
{
    std::string sName;
    MathKind eKind;
    int iFn;
};

static std::vector<MathForm> BuildForms()
{
    std::vector<MathForm> forms;
    for (int i = 0; i < UNARY_COUNT; ++i)
    {
        MathForm kF = {std::string(sUnary[i].pcName) + ".f", MK_F1, i};
        MathForm kD = {std::string(sUnary[i].pcName) + ".d", MK_D1, i};
        forms.push_back(kF);
        forms.push_back(kD);
    }
    for (int i = 0; i < BINARY_COUNT; ++i)
    {
        bool bPow = strcmp(sBinary[i].pcName, "pow") == 0;
        MathForm kF = {std::string(sBinary[i].pcName) + (bPow ? ".ff" : ".f"), MK_F2, i};
        MathForm kD = {std::string(sBinary[i].pcName) + (bPow ? ".dd" : ".d"), MK_D2, i};
        forms.push_back(kF);
        forms.push_back(kD);
        if (bPow)
        {
            MathForm kFI = {"pow.fi", MK_FI, i};
            MathForm kDI = {"pow.di", MK_DI, i};
            forms.push_back(kFI);
            forms.push_back(kDI);
        }
    }
    return forms;
}

static const std::vector<MathForm>& Forms()
{
    static std::vector<MathForm> sForms = BuildForms();
    return sForms;
}

static const MathForm* FindForm(const std::string& sName)
{
    const std::vector<MathForm>& forms = Forms();
    for (size_t i = 0; i < forms.size(); ++i)
    {
        if (forms[i].sName == sName)
        {
            return &forms[i];
        }
    }
    return 0;
}

static float BitsToF(unsigned int uBits)
{
    float f;
    memcpy(&f, &uBits, 4);
    return f;
}

static double BitsToD(u64 uBits)
{
    double d;
    memcpy(&d, &uBits, 8);
    return d;
}

static unsigned int FToBits(float f)
{
    unsigned int u;
    memcpy(&u, &f, 4);
    return u;
}

static u64 DToBits(double d)
{
    u64 u;
    memcpy(&u, &d, 8);
    return u;
}

static bool IsDoubleKind(MathKind eKind)
{
    return eKind == MK_D1 || eKind == MK_D2 || eKind == MK_DI;
}

static std::string MathLine(const MathForm& form, u64 uIn1, u64 uIn2)
{
    char acIn[64];
    char acOut[32];
    switch (form.eKind)
    {
    case MK_F1:
    {
        float fOut = sUnary[form.iFn].pfnF(BitsToF((unsigned int)uIn1));
        _snprintf(acIn, sizeof(acIn), "%08X", (unsigned int)uIn1);
        _snprintf(acOut, sizeof(acOut), "%08X", FToBits(fOut));
        break;
    }
    case MK_D1:
    {
        double dOut = sUnary[form.iFn].pfnD(BitsToD(uIn1));
        _snprintf(acIn, sizeof(acIn), "%016I64X", uIn1);
        _snprintf(acOut, sizeof(acOut), "%016I64X", DToBits(dOut));
        break;
    }
    case MK_F2:
    {
        float fOut = sBinary[form.iFn].pfnF(BitsToF((unsigned int)uIn1), BitsToF((unsigned int)uIn2));
        _snprintf(acIn, sizeof(acIn), "%08X %08X", (unsigned int)uIn1, (unsigned int)uIn2);
        _snprintf(acOut, sizeof(acOut), "%08X", FToBits(fOut));
        break;
    }
    case MK_D2:
    {
        double dOut = sBinary[form.iFn].pfnD(BitsToD(uIn1), BitsToD(uIn2));
        _snprintf(acIn, sizeof(acIn), "%016I64X %016I64X", uIn1, uIn2);
        _snprintf(acOut, sizeof(acOut), "%016I64X", DToBits(dOut));
        break;
    }
    case MK_FI:
    {
        int iExp = (int)uIn2;
        float fOut = pow(BitsToF((unsigned int)uIn1), iExp);
        _snprintf(acIn, sizeof(acIn), "%08X %d", (unsigned int)uIn1, iExp);
        _snprintf(acOut, sizeof(acOut), "%08X", FToBits(fOut));
        break;
    }
    default:
    {
        int iExp = (int)uIn2;
        double dOut = pow(BitsToD(uIn1), iExp);
        _snprintf(acIn, sizeof(acIn), "%016I64X %d", uIn1, iExp);
        _snprintf(acOut, sizeof(acOut), "%016I64X", DToBits(dOut));
        break;
    }
    }
    acIn[sizeof(acIn) - 1] = 0;
    acOut[sizeof(acOut) - 1] = 0;
    return form.sName + "\t" + acIn + "\t" + acOut;
}

static bool ParseMathLine(const std::string& sLine, std::string& sRebuilt)
{
    size_t uTab1 = sLine.find('\t');
    if (uTab1 == std::string::npos)
    {
        return false;
    }
    size_t uTab2 = sLine.find('\t', uTab1 + 1);
    if (uTab2 == std::string::npos || sLine.find('\t', uTab2 + 1) != std::string::npos)
    {
        return false;
    }
    const MathForm* pForm = FindForm(sLine.substr(0, uTab1));
    if (!pForm)
    {
        return false;
    }
    std::string sIn = sLine.substr(uTab1 + 1, uTab2 - uTab1 - 1);
    unsigned int uF1 = 0;
    unsigned int uF2 = 0;
    u64 uD1 = 0;
    u64 uD2 = 0;
    int iExp = 0;
    u64 uIn1 = 0;
    u64 uIn2 = 0;
    switch (pForm->eKind)
    {
    case MK_F1:
        if (sscanf(sIn.c_str(), "%8x", &uF1) != 1)
        {
            return false;
        }
        uIn1 = uF1;
        break;
    case MK_D1:
        if (sscanf(sIn.c_str(), "%16I64x", &uD1) != 1)
        {
            return false;
        }
        uIn1 = uD1;
        break;
    case MK_F2:
        if (sscanf(sIn.c_str(), "%8x %8x", &uF1, &uF2) != 2)
        {
            return false;
        }
        uIn1 = uF1;
        uIn2 = uF2;
        break;
    case MK_D2:
        if (sscanf(sIn.c_str(), "%16I64x %16I64x", &uD1, &uD2) != 2)
        {
            return false;
        }
        uIn1 = uD1;
        uIn2 = uD2;
        break;
    case MK_FI:
        if (sscanf(sIn.c_str(), "%8x %d", &uF1, &iExp) != 2)
        {
            return false;
        }
        uIn1 = uF1;
        uIn2 = (u64)(unsigned int)iExp;
        break;
    default:
        if (sscanf(sIn.c_str(), "%16I64x %d", &uD1, &iExp) != 2)
        {
            return false;
        }
        uIn1 = uD1;
        uIn2 = (u64)(unsigned int)iExp;
        break;
    }
    sRebuilt = MathLine(*pForm, uIn1, uIn2);
    return true;
}

static std::string ParseFields(const char* pcToken)
{
    unsigned int uScan = 0x7FA5A5A5u;
    int iRet = sscanf(pcToken, "%f", (float*)&uScan);
    double dAtof = atof(pcToken);
    float fAtof = (float)atof(pcToken);
    char* pcEnd = 0;
    double dStrtod = strtod(pcToken, &pcEnd);
    char acOut[128];
    _snprintf(acOut, sizeof(acOut), "%d %08X %016I64X %08X %016I64X %d", iRet, uScan, DToBits(dAtof), FToBits(fAtof),
              DToBits(dStrtod), (int)(pcEnd - pcToken));
    acOut[sizeof(acOut) - 1] = 0;
    return acOut;
}

static bool ParseTokenLine(const std::string& sLine, std::string& sExpected, std::string& sActual)
{
    size_t uTab = sLine.find('\t');
    if (uTab == std::string::npos || uTab == 0 || uTab > 255)
    {
        return false;
    }
    std::string sToken = sLine.substr(0, uTab);
    sExpected = sLine.substr(uTab + 1);
    sActual = ParseFields(sToken.c_str());
    return true;
}

static bool WriteLines(const std::string& sPath, const std::vector<std::string>& lines)
{
    FILE* pFile = fopen(sPath.c_str(), "wb");
    if (!pFile)
    {
        printf("gen-corpus: cannot write %s\n", sPath.c_str());
        return false;
    }
    for (size_t i = 0; i < lines.size(); ++i)
    {
        fputs(lines[i].c_str(), pFile);
        fputs("\r\n", pFile);
    }
    bool bOk = ferror(pFile) == 0;
    bOk = fclose(pFile) == 0 && bOk;
    return bOk;
}

static std::vector<float> UnaryEdges()
{
    static const unsigned int auBits[] = {0x00000000u, 0x80000000u, 0x00800000u, 0x80800000u, 0x00000001u, 0x80000001u,
                                          0x7F7FFFFFu, 0xFF7FFFFFu, 0x3F800000u, 0xBF800000u, 0x3F000000u, 0x7FC00000u,
                                          0x7F800000u, 0xFF800000u};
    std::vector<float> values;
    for (size_t i = 0; i < sizeof(auBits) / sizeof(auBits[0]); ++i)
    {
        values.push_back(BitsToF(auBits[i]));
    }
    static const double adMul[] = {0.5, 1.0, -1.0, 2.0, 3.0, 100.0};
    for (size_t i = 0; i < sizeof(adMul) / sizeof(adMul[0]); ++i)
    {
        values.push_back((float)(kPi * adMul[i]));
    }
    values.push_back(1e6f);
    values.push_back(1e10f);
    values.push_back(-1e10f);
    return values;
}

static std::vector<float> PairEdges()
{
    static const unsigned int auBits[] = {0x00000000u, 0x80000000u, 0x3F800000u, 0xBF800000u, 0x3F000000u, 0x40000000u,
                                          0x00800000u, 0x00000001u, 0x7F7FFFFFu, 0x7F800000u, 0xFF800000u, 0x7FC00000u};
    std::vector<float> values;
    for (size_t i = 0; i < sizeof(auBits) / sizeof(auBits[0]); ++i)
    {
        values.push_back(BitsToF(auBits[i]));
    }
    return values;
}

static std::vector<double> DoubleExtras()
{
    std::vector<double> values;
    values.push_back(1e300);
    values.push_back(-1e300);
    values.push_back(DBL_MIN);
    values.push_back(BitsToD(1));
    values.push_back(DBL_MAX);
    values.push_back(kPi);
    values.push_back(kPi * 0.5);
    return values;
}

static std::vector<float> Grid(double dLo, double dHi, int iCount)
{
    std::vector<float> values;
    for (int i = 0; i < iCount; ++i)
    {
        values.push_back((float)(dLo + (dHi - dLo) * i / (iCount - 1)));
    }
    return values;
}

static void AppendUnary(std::vector<std::string>& lines, const MathForm& form)
{
    const UnaryFn& kFn = sUnary[form.iFn];
    std::vector<float> values = Grid(kFn.dLo, kFn.dHi, 256);
    std::vector<float> edges = UnaryEdges();
    values.insert(values.end(), edges.begin(), edges.end());
    for (size_t i = 0; i < values.size(); ++i)
    {
        u64 uIn = form.eKind == MK_F1 ? (u64)FToBits(values[i]) : DToBits((double)values[i]);
        lines.push_back(MathLine(form, uIn, 0));
    }
    if (form.eKind == MK_D1)
    {
        std::vector<double> extras = DoubleExtras();
        for (size_t i = 0; i < extras.size(); ++i)
        {
            lines.push_back(MathLine(form, DToBits(extras[i]), 0));
        }
    }
}

static void AppendPair(std::vector<std::string>& lines, const MathForm& form, float f1, float f2)
{
    if (IsDoubleKind(form.eKind))
    {
        lines.push_back(MathLine(form, DToBits((double)f1), DToBits((double)f2)));
    }
    else
    {
        lines.push_back(MathLine(form, FToBits(f1), FToBits(f2)));
    }
}

static void AppendBinary(std::vector<std::string>& lines, const MathForm& form)
{
    const BinaryFn& kFn = sBinary[form.iFn];
    std::vector<float> grid1 = Grid(kFn.dLo1, kFn.dHi1, 16);
    std::vector<float> grid2 = Grid(kFn.dLo2, kFn.dHi2, 16);
    for (size_t i = 0; i < grid1.size(); ++i)
    {
        for (size_t j = 0; j < grid2.size(); ++j)
        {
            AppendPair(lines, form, grid1[i], grid2[j]);
        }
    }
    std::vector<float> edges = PairEdges();
    for (size_t i = 0; i < edges.size(); ++i)
    {
        for (size_t j = 0; j < edges.size(); ++j)
        {
            AppendPair(lines, form, edges[i], edges[j]);
        }
    }
    if (form.eKind == MK_D2)
    {
        std::vector<double> extras = DoubleExtras();
        std::vector<float> others = PairEdges();
        for (size_t i = 0; i < extras.size(); ++i)
        {
            for (size_t j = 0; j < others.size(); ++j)
            {
                lines.push_back(MathLine(form, DToBits(extras[i]), DToBits((double)others[j])));
                lines.push_back(MathLine(form, DToBits((double)others[j]), DToBits(extras[i])));
            }
        }
    }
}

static void AppendPowInt(std::vector<std::string>& lines, const MathForm& form)
{
    std::vector<float> bases = Grid(-10.0, 10.0, 16);
    std::vector<float> edges = PairEdges();
    bases.insert(bases.end(), edges.begin(), edges.end());
    for (size_t i = 0; i < bases.size(); ++i)
    {
        for (int iExp = -8; iExp <= 8; ++iExp)
        {
            u64 uIn = form.eKind == MK_FI ? (u64)FToBits(bases[i]) : DToBits((double)bases[i]);
            lines.push_back(MathLine(form, uIn, (u64)(unsigned int)iExp));
        }
    }
}

static std::vector<std::string> BuildMathLines()
{
    std::vector<std::string> lines;
    const std::vector<MathForm>& forms = Forms();
    for (size_t i = 0; i < forms.size(); ++i)
    {
        switch (forms[i].eKind)
        {
        case MK_F1:
        case MK_D1:
            AppendUnary(lines, forms[i]);
            break;
        case MK_F2:
        case MK_D2:
            AppendBinary(lines, forms[i]);
            break;
        default:
            AppendPowInt(lines, forms[i]);
            break;
        }
    }
    return lines;
}

static const char* const sEdgeTokens[] = {
    "0", "1", "-1", "0.1", "1.0000001", "3.14159265358979323846264338327950288",
    "0.333333333333333333333333333333", "123456789012345678901234567890", "16777217", "16777216.5",
    "0.100000001490116119384765625", "1.000000059604644775390625", "1.0000000596046447753906250001",
    "1.0000000596046447753906249999", "3.4028234663852886e38", "3.4028235677973366e38", "1e38", "1e39", "1e-46",
    "1e-45", "1.4e-45", "7e-46", "1.17549435e-38", "1.17549421e-38", "1E+5", "1e+37", "+1.5", "+.5", ".5", "-.05",
    "-0", "-0.0", "00000000000000000001.5", "9999999999999999999999999999999999999999", "2.2250738585072014e-308",
    "4.9e-324", "1e-400", "1e400", "13213454682", "-0.8333333", "0x1p3", "0x10", "1.5f", "2.0,", "3abc", "1e", "1e+",
    "1.5e-x", ".", "e5", "-", "+", "nan", "NAN", "inf", "-inf", "infinity", "1.#INF", "-1.#INF", "1.#QNAN", "1.#IND",
};

static bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool IsNumericToken(const std::string& sToken)
{
    size_t i = 0;
    size_t uLen = sToken.size();
    if (i < uLen && (sToken[i] == '+' || sToken[i] == '-'))
    {
        ++i;
    }
    size_t uIntStart = i;
    while (i < uLen && IsDigit(sToken[i]))
    {
        ++i;
    }
    bool bInt = i > uIntStart;
    if (i < uLen && sToken[i] == '.')
    {
        ++i;
        size_t uFracStart = i;
        while (i < uLen && IsDigit(sToken[i]))
        {
            ++i;
        }
        if (!bInt && i == uFracStart)
        {
            return false;
        }
    }
    else if (!bInt)
    {
        return false;
    }
    if (i < uLen && (sToken[i] == 'e' || sToken[i] == 'E'))
    {
        ++i;
        if (i < uLen && (sToken[i] == '+' || sToken[i] == '-'))
        {
            ++i;
        }
        size_t uExpStart = i;
        while (i < uLen && IsDigit(sToken[i]))
        {
            ++i;
        }
        if (i == uExpStart)
        {
            return false;
        }
    }
    return i == uLen;
}

static void AddPropTokens(const char* pcValue, std::set<std::string>& tokens)
{
    std::string sToken;
    for (const char* pc = pcValue;; ++pc)
    {
        char c = *pc;
        if (c == 0 || c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            if (!sToken.empty() && IsNumericToken(sToken))
            {
                tokens.insert(sToken);
            }
            sToken.clear();
            if (c == 0)
            {
                break;
            }
        }
        else if (c != '"' && c != ',')
        {
            sToken += c;
        }
    }
}

static unsigned int ReadU32(const unsigned char* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24);
}

struct OdfStats
{
    unsigned int uFiles;
    unsigned int uProps;
    unsigned int uSkipped;
};

static bool WalkClass(const unsigned char* pBase, size_t uStart, size_t uEnd, std::set<std::string>& tokens,
                      OdfStats& stats)
{
    size_t uPos = uStart;
    while (uPos + 8 <= uEnd)
    {
        unsigned int uSize = ReadU32(pBase + uPos + 4);
        size_t uData = uPos + 8;
        if (uSize > uEnd - uData)
        {
            return false;
        }
        if (memcmp(pBase + uPos, "PROP", 4) == 0 && uSize >= 4)
        {
            char acValue[256];
            size_t uLen = 0;
            const unsigned char* pStr = pBase + uData + 4;
            size_t uMax = uSize - 4;
            while (uLen < uMax && uLen < 255 && pStr[uLen] != 0)
            {
                acValue[uLen] = (char)pStr[uLen];
                ++uLen;
            }
            acValue[uLen] = 0;
            ++stats.uProps;
            AddPropTokens(acValue, tokens);
        }
        uPos = uData + uSize;
        uPos += (4 - (uPos & 3)) & 3;
    }
    return true;
}

static bool WalkLevel(const unsigned char* pBase, size_t uStart, size_t uEnd, std::set<std::string>& tokens,
                      OdfStats& stats, int iDepth)
{
    if (iDepth > 16)
    {
        return false;
    }
    size_t uPos = uStart;
    while (uPos + 8 <= uEnd)
    {
        unsigned int uSize = ReadU32(pBase + uPos + 4);
        size_t uData = uPos + 8;
        if (uSize > uEnd - uData)
        {
            return false;
        }
        const unsigned char* pId = pBase + uPos;
        if (memcmp(pId, "lvl_", 4) == 0)
        {
            size_t uChild = uData;
            size_t uChildEnd = uData + uSize;
            while (uChild + 8 <= uChildEnd)
            {
                unsigned int uChildSize = ReadU32(pBase + uChild + 4);
                size_t uChildData = uChild + 8;
                if (uChildSize > uChildEnd - uChildData)
                {
                    return false;
                }
                if (!WalkLevel(pBase, uChildData, uChildData + uChildSize, tokens, stats, iDepth + 1))
                {
                    return false;
                }
                uChild = uChildData + uChildSize;
                uChild += (4 - (uChild & 3)) & 3;
            }
        }
        else if (memcmp(pId, "entc", 4) == 0 || memcmp(pId, "ordc", 4) == 0 || memcmp(pId, "wpnc", 4) == 0
                 || memcmp(pId, "expc", 4) == 0)
        {
            if (!WalkClass(pBase, uData, uData + uSize, tokens, stats))
            {
                return false;
            }
        }
        uPos = uData + uSize;
        uPos += (4 - (uPos & 3)) & 3;
    }
    return true;
}

static bool EndsWithLvl(const char* pcName)
{
    size_t uLen = strlen(pcName);
    return uLen > 4 && _stricmp(pcName + uLen - 4, ".lvl") == 0;
}

static void ListLvlFiles(const std::string& sDir, std::vector<std::string>& files)
{
    WIN32_FIND_DATAA find;
    HANDLE hFind = FindFirstFileA((sDir + "\\*").c_str(), &find);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if (strcmp(find.cFileName, ".") == 0 || strcmp(find.cFileName, "..") == 0)
        {
            continue;
        }
        std::string sPath = sDir + "\\" + find.cFileName;
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ListLvlFiles(sPath, files);
        }
        else if (EndsWithLvl(find.cFileName))
        {
            files.push_back(sPath);
        }
    } while (FindNextFileA(hFind, &find));
    FindClose(hFind);
}

static bool ReadWholeFile(const std::string& sPath, std::vector<unsigned char>& bytes)
{
    bytes.clear();
    FILE* pFile = fopen(sPath.c_str(), "rb");
    if (!pFile)
    {
        return false;
    }
    bool bOk = fseek(pFile, 0, SEEK_END) == 0;
    long lSize = bOk ? ftell(pFile) : -1;
    bOk = lSize >= 0 && fseek(pFile, 0, SEEK_SET) == 0;
    if (bOk && lSize > 0)
    {
        bytes.resize((size_t)lSize);
        bOk = fread(&bytes[0], 1, (size_t)lSize, pFile) == (size_t)lSize;
    }
    fclose(pFile);
    return bOk;
}

static bool BuildOdfLines(const char* pcRoot, std::vector<std::string>& lines)
{
    std::vector<std::string> files;
    ListLvlFiles(std::string(pcRoot) + "\\data\\_lvl_pc", files);
    std::sort(files.begin(), files.end());
    std::set<std::string> tokens;
    OdfStats stats = {0, 0, 0};
    for (size_t i = 0; i < files.size(); ++i)
    {
        std::vector<unsigned char> bytes;
        if (!ReadWholeFile(files[i], bytes) || bytes.size() < 8 || memcmp(&bytes[0], "ucfb", 4) != 0)
        {
            printf("gen-corpus: skipped %s (not a ucfb file)\n", files[i].c_str());
            ++stats.uSkipped;
            continue;
        }
        unsigned int uSize = ReadU32(&bytes[4]);
        size_t uEnd = uSize > bytes.size() - 8 ? bytes.size() : 8 + (size_t)uSize;
        std::set<std::string> fileTokens;
        OdfStats fileStats = {0, 0, 0};
        if (!WalkLevel(&bytes[0], 8, uEnd, fileTokens, fileStats, 0))
        {
            printf("gen-corpus: skipped %s (malformed chunk)\n", files[i].c_str());
            ++stats.uSkipped;
            continue;
        }
        ++stats.uFiles;
        stats.uProps += fileStats.uProps;
        tokens.insert(fileTokens.begin(), fileTokens.end());
    }
    for (std::set<std::string>::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        lines.push_back(*it + "\t" + ParseFields(it->c_str()));
    }
    printf("gen-corpus: lvl files=%u skipped=%u props=%u tokens=%u\n", stats.uFiles, stats.uSkipped, stats.uProps,
           (unsigned int)tokens.size());
    return stats.uFiles > 0;
}

int FloatCorpusGenerate(const char* pcOutDir)
{
    const char* pcRoot = FindGameDataRoot();
    if (!pcRoot)
    {
        printf("[SKIP] gen-corpus: GameData not found\n");
        fflush(stdout);
        return 77;
    }
    std::string sOut = pcOutDir;
    CreateDirectoryA(sOut.c_str(), 0);
    CreateDirectoryA((sOut + "\\floatparse").c_str(), 0);
    CreateDirectoryA((sOut + "\\mathfn").c_str(), 0);

    std::vector<std::string> mathLines = BuildMathLines();
    if (!WriteLines(sOut + "\\mathfn\\math.txt", mathLines))
    {
        return 1;
    }
    printf("gen-corpus: math lines=%u\n", (unsigned int)mathLines.size());

    std::vector<std::string> edgeLines;
    for (size_t i = 0; i < sizeof(sEdgeTokens) / sizeof(sEdgeTokens[0]); ++i)
    {
        edgeLines.push_back(std::string(sEdgeTokens[i]) + "\t" + ParseFields(sEdgeTokens[i]));
    }
    if (!WriteLines(sOut + "\\floatparse\\edge.txt", edgeLines))
    {
        return 1;
    }
    printf("gen-corpus: edge tokens=%u\n", (unsigned int)edgeLines.size());

    std::vector<std::string> odfLines;
    if (!BuildOdfLines(pcRoot, odfLines) || !WriteLines(sOut + "\\floatparse\\odf.txt", odfLines))
    {
        return 1;
    }
    fflush(stdout);
    return 0;
}

static std::vector<std::string> ReadCorpusLines(const char* pcPath, unsigned int& uBad)
{
    std::vector<std::string> lines;
    uBad = 0;
    FILE* pFile = fopen(pcPath, "rb");
    if (!pFile)
    {
        return lines;
    }
    char acLine[512];
    bool bContinuation = false;
    while (fgets(acLine, sizeof(acLine), pFile))
    {
        size_t uLen = strlen(acLine);
        bool bComplete = uLen > 0 && acLine[uLen - 1] == '\n';
        if (!bComplete && !feof(pFile))
        {
            if (!bContinuation)
            {
                ++uBad;
            }
            bContinuation = true;
            continue;
        }
        if (bContinuation)
        {
            bContinuation = false;
            continue;
        }
        while (uLen > 0 && (acLine[uLen - 1] == '\n' || acLine[uLen - 1] == '\r'))
        {
            acLine[--uLen] = 0;
        }
        lines.push_back(acLine);
    }
    fclose(pFile);
    return lines;
}

static std::string LineLabel(const char* pcPath, size_t uIndex, const std::string& sLine)
{
    char acNum[16];
    _snprintf(acNum, sizeof(acNum), ":%u ", (unsigned int)(uIndex + 1));
    acNum[sizeof(acNum) - 1] = 0;
    return std::string(pcPath) + acNum + sLine;
}

static void CheckParseCorpus(const char* pcPath, size_t uMinLines)
{
    WatchdogArm(pcPath);
    unsigned int uBad = 0;
    std::vector<std::string> lines = ReadCorpusLines(pcPath, uBad);
    CHECK(uBad == 0);
    REQUIRE(lines.size() >= uMinLines);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string sWhere = LineLabel(pcPath, i, lines[i]);
        INFO(sWhere);
        std::string sExpected;
        std::string sActual;
        bool bParsed = ParseTokenLine(lines[i], sExpected, sActual);
        CHECK(bParsed);
        if (bParsed)
        {
            CHECK(sActual == sExpected);
        }
    }
}

static bool IsX87PrecisionForm(const std::string& sLine)
{
    return sLine.compare(0, 7, "sqrt.d\t") == 0 || sLine.compare(0, 7, "atan.d\t") == 0;
}

static unsigned int CheckMathCorpus(bool bAllowX87PrecisionForms)
{
    const char* pcPath = "corpus\\mathfn\\math.txt";
    WatchdogArm(pcPath);
    unsigned int uBad = 0;
    unsigned int uX87 = 0;
    std::vector<std::string> lines = ReadCorpusLines(pcPath, uBad);
    CHECK(uBad == 0);
    REQUIRE(lines.size() >= 4000);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string sWhere = LineLabel(pcPath, i, lines[i]);
        INFO(sWhere);
        std::string sRebuilt;
        bool bParsed = ParseMathLine(lines[i], sRebuilt);
        CHECK(bParsed);
        if (!bParsed || sRebuilt == lines[i])
        {
            continue;
        }
        if (bAllowX87PrecisionForms && IsX87PrecisionForm(lines[i]))
        {
            ++uX87;
            continue;
        }
        CHECK(sRebuilt == lines[i]);
    }
    return uX87;
}

TEST_CASE("float parse corpus odf" * doctest::test_suite("corpus"))
{
    CheckParseCorpus("corpus\\floatparse\\odf.txt", 700);
}

TEST_CASE("float parse corpus edge" * doctest::test_suite("corpus"))
{
    CheckParseCorpus("corpus\\floatparse\\edge.txt", 30);
}

TEST_CASE("math corpus" * doctest::test_suite("corpus"))
{
    CHECK(CheckMathCorpus(false) == 0);
}

struct PrecisionGuard
{
    unsigned int uOld;
    ~PrecisionGuard()
    {
        unsigned int uTmp = 0;
        _controlfp_s(&uTmp, uOld & _MCW_PC, _MCW_PC);
    }
};

TEST_CASE("math corpus x87 pc24" * doctest::test_suite("corpus"))
{
    unsigned int uOld = 0;
    unsigned int uTmp = 0;
    REQUIRE(_controlfp_s(&uOld, 0, 0) == 0);
    PrecisionGuard kGuard = {uOld};
    REQUIRE(_controlfp_s(&uTmp, _PC_24, _MCW_PC) == 0);
    unsigned int uX87 = CheckMathCorpus(true);
    printf("math corpus x87 pc24: %u sqrt.d/atan.d lines follow x87 precision control\n", uX87);
#ifndef OPENSWBF2_TESTS_DEBUG
    CHECK(uX87 == 0);
#endif
}
