#pragma once

#include "RedEngineFL/Util/RedWarning.h"

namespace pcCrash
{
void __cdecl Check(char const* pcExpr, char const* pcFile, int iLine);
}

namespace RedLog
{

enum Category
{
    LogNet = 0,
    LogLua,
    LogLoad,
    LogMemory,
    LogSim,
    LogRender,
    LogAudio,
    LogCore,
    LogTemp,
    CategoryCount
};

enum Severity
{
    Verbose = RedWarning::RED_SEVERITY_VERBOSE,
    Inform = RedWarning::RED_SEVERITY_INFORM,
    Warning = RedWarning::RED_SEVERITY_WARNING,
    Error = RedWarning::RED_SEVERITY_ERROR,
    Bug = RedWarning::RED_SEVERITY_BUG,
    Fatal = RedWarning::RED_SEVERITY_FATAL
};

typedef void(__cdecl* Observer)(int iSeverity, int iCategory, char const* pcLine);

extern Observer gObserver;
extern int gFrame;

void __cdecl Write(Category eCategory, Severity eSeverity, char const* pcFile, int iLine,
                   char const* pcFormat, ...);
void NextFrame();
// Also write every log line to stdout (dedicated servers: console, launcher pipe, container
// log). A process without stdout then attaches to its parent's console (StdoutAttach) or
// attaches or opens one (StdoutConsole).
enum StdoutMode
{
    StdoutOff,
    StdoutRedirectOnly,
    StdoutAttach,
    StdoutConsole
};
void SetStdout(StdoutMode eMode);
void ParseCmds(char const* pcArgs);
void Apply(char const* pcBuildId);
int MinVerbosity();
char const* CategoryName(int iCategory);
char const* SeverityName(int iSeverity);
char const* TrimPath(char const* pcPath);
bool EnsureFailed(char const* pcExpr, char const* pcFile, int iLine);
unsigned int EnsureFailureCount();
unsigned int EnsureSiteCount();
void LogEnsureSummary();
void ResetForTest();

}

#define LOG(cat, sev, ...) RedLog::Write(RedLog::cat, RedLog::sev, __FILE__, __LINE__, "" __VA_ARGS__)

#define ensure(expr) ((expr) ? true : RedLog::EnsureFailed(#expr, __FILE__, __LINE__))

#define check(expr) ((expr) ? (void)0 : pcCrash::Check(#expr, __FILE__, __LINE__))
