#include <windows.h>
#include <string.h>
#include <io.h>
#include <sys/stat.h>

#include "testutil.h"
#include "RedEngineFL/Util/RedWarning.h"
#include "RedEngineFL/Util/RedLog.h"

std::vector<std::string> LogCapture::sLines;

LogCapture::LogCapture()
    : mpOldCallback(RedWarning::g_fnLogCallback)
    , meOldSeverity((int)RedWarning::GetSeverityToDestination(RED_LOG_DESTINATION_INDEX_CALLBACK))
{
    sLines.clear();
    RedWarning::g_fnLogCallback = &LogCapture::Sink;
    RedWarning::SetSeverityToDestination(RED_LOG_DESTINATION_INDEX_CALLBACK, RedWarning::RED_SEVERITY_VERBOSE);
}

LogCapture::~LogCapture()
{
    RedWarning::SetSeverityToDestination(RED_LOG_DESTINATION_INDEX_CALLBACK,
                                         (RedWarning::RED_ERROR_SEVERITY)meOldSeverity);
    RedWarning::g_fnLogCallback = mpOldCallback;
}

bool LogCapture::Contains(const char* pcText) const
{
    for (size_t i = 0; i < sLines.size(); ++i)
    {
        if (sLines[i].find(pcText) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

int LogCapture::Count(const char* pcText) const
{
    int iCount = 0;
    for (size_t i = 0; i < sLines.size(); ++i)
    {
        if (sLines[i].find(pcText) != std::string::npos)
        {
            ++iCount;
        }
    }
    return iCount;
}

void __cdecl LogCapture::Sink(char const* pcLine, ...)
{
    sLines.push_back(pcLine);
}

struct Expectation
{
    std::string sText;
    bool bMatched;
};

static std::vector<Expectation> sExpectations;
static std::vector<std::string> sUndeclared;
static std::vector<std::string> sUnmatched;

ExpectLog::ExpectLog(const char* pcSubstring)
    : mText(pcSubstring)
{
    Expectation entry;
    entry.sText = mText;
    entry.bMatched = false;
    sExpectations.push_back(entry);
}

ExpectLog::~ExpectLog()
{
    if (!sExpectations.back().bMatched)
    {
        sUnmatched.push_back(mText + " (declared but never logged)");
    }
    sExpectations.pop_back();
}

ExpectEnsure::ExpectEnsure(const char* pcName)
    : mLog((std::string("ensure failed: ") + pcName).c_str())
{
}

void __cdecl TestLogObserver(int iSeverity, int, char const* pcLine)
{
    if (iSeverity < RedLog::Error)
    {
        return;
    }
    for (size_t i = sExpectations.size(); i > 0; --i)
    {
        if (strstr(pcLine, sExpectations[i - 1].sText.c_str()))
        {
            sExpectations[i - 1].bMatched = true;
            return;
        }
    }
    sUndeclared.push_back(pcLine);
}

int TestStrictnessReport(const char* pcCase)
{
    int iCount = 0;
    for (size_t i = 0; i < sUndeclared.size(); ++i)
    {
        printf("[UNEXPECTED-LOG] %s: %s", pcCase, sUndeclared[i].c_str());
        ++iCount;
    }
    for (size_t i = 0; i < sUnmatched.size(); ++i)
    {
        printf("[UNEXPECTED-LOG] %s: %s\n", pcCase, sUnmatched[i].c_str());
        ++iCount;
    }
    fflush(stdout);
    return iCount;
}

static bool DirExists(const std::string& sPath)
{
    DWORD dwAttr = GetFileAttributesA(sPath.c_str());
    return dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool ReadInstallLocation(REGSAM samView, std::string& sOut)
{
    HKEY hKey = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 6060",
                      0, KEY_READ | samView, &hKey) != ERROR_SUCCESS)
    {
        return false;
    }
    char acBuf[MAX_PATH] = {0};
    DWORD dwSize = sizeof(acBuf) - 1;
    DWORD dwType = 0;
    LONG lResult = RegQueryValueExA(hKey, "InstallLocation", 0, &dwType, (LPBYTE)acBuf, &dwSize);
    RegCloseKey(hKey);
    if (lResult != ERROR_SUCCESS || dwType != REG_SZ)
    {
        return false;
    }
    acBuf[dwSize < sizeof(acBuf) ? dwSize : sizeof(acBuf) - 1] = 0;
    sOut = acBuf;
    return !sOut.empty();
}

const char* FindGameDataRoot()
{
    static std::string sRoot;
    static int iState = 0;
    if (iState == 0)
    {
        iState = 2;
        const char* pcEnv = getenv("SWBF2_GAMEDATA");
        std::string sCandidate;
        if (pcEnv && *pcEnv)
        {
            sCandidate = pcEnv;
        }
        else if (ReadInstallLocation(KEY_WOW64_64KEY, sCandidate) || ReadInstallLocation(KEY_WOW64_32KEY, sCandidate))
        {
            sCandidate += "\\GameData";
        }
        if (!sCandidate.empty() && DirExists(sCandidate + "\\data\\_lvl_pc"))
        {
            sRoot = sCandidate;
            iState = 1;
        }
    }
    return iState == 1 ? sRoot.c_str() : 0;
}

void SkipCase(const char* pcReason)
{
    printf("[SKIP] %s: %s\n", gCurrentCase, pcReason);
    fflush(stdout);
    _exit(77);
}

void HarnessError(const char* pcReason)
{
    printf("[HARNESS] %s: %s\n", gCurrentCase, pcReason);
    fflush(stdout);
    _exit(3);
}

bool ReadRepoText(const char* pcRelPath, std::string& sOut)
{
    char acExe[MAX_PATH];
    DWORD uLen = GetModuleFileNameA(0, acExe, sizeof(acExe));
    if (uLen == 0 || uLen >= sizeof(acExe))
    {
        return false;
    }
    // the repo root is the nearest folder above the exe that holds BattlefrontII.sln
    // (bin\Tests\<Config> on Win32, bin\Tests\x64\<Config> on x64)
    std::string sPath(acExe, uLen);
    for (;;)
    {
        size_t uiSlash = sPath.find_last_of("\\/");
        if (uiSlash == std::string::npos)
        {
            return false;
        }
        sPath.erase(uiSlash);
        FILE* pSln = fopen((sPath + "\\BattlefrontII.sln").c_str(), "rb");
        if (pSln)
        {
            fclose(pSln);
            break;
        }
    }
    sPath += "\\";
    sPath += pcRelPath;
    FILE* pFile = fopen(sPath.c_str(), "rb");
    if (!pFile)
    {
        return false;
    }
    sOut.clear();
    char acBuf[4096];
    size_t uiRead;
    while ((uiRead = fread(acBuf, 1, sizeof(acBuf), pFile)) > 0)
    {
        for (size_t i = 0; i < uiRead; ++i)
        {
            if (acBuf[i] != '\r')
            {
                sOut += acBuf[i];
            }
        }
    }
    fclose(pFile);
    return true;
}
