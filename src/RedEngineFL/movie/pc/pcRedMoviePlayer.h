#pragma once

#include "RedEngineFL/movie/RedMoviePlayerBase.h"
#include "RedEngineFL/movie/RedMoviePlayerRenderer.h"

#ifdef OPENSWBF2_FFMPEG

// x64 has no binkw32: FFmpeg decodes the movies and a DirectSound buffer plays their audio
#include "RedEngineFL/movie/pc/FFMovieDecoder.h"

struct IDirectSoundBuffer;

#else

struct BINK
{
    unsigned long Width;
    unsigned long Height;
    unsigned long Frames;
    unsigned long FrameNum;
    unsigned long LastFrameNum;
    unsigned long FrameRate;
    unsigned long FrameRateDiv;
    unsigned char pad1c[0xd0 - 0x1c];
    long NumTracks;
};

struct BINKFILE
{
    unsigned long fileHandle;
    unsigned long fileIOPos;
    unsigned long fileBufPos;
    unsigned char* bufPos;
    unsigned long bufEmpty;
    long inIdle;
    unsigned char* buffer;
    unsigned char* bufEnd;
    unsigned char* bufBack;
    unsigned long dontClose;
    unsigned long startFile;
    unsigned long fileSize;
    unsigned long simulate;
    long adjustRate;
};

struct BINKIO
{
    unsigned long(__stdcall* ReadHeader)(BINKIO*, long, void*, unsigned long);
    unsigned long(__stdcall* ReadFrame)(BINKIO*, unsigned long, long, void*, unsigned long);
    unsigned long(__stdcall* GetBufferSize)(BINKIO*, unsigned long);
    void(__stdcall* SetInfo)(BINKIO*, void*, unsigned long, unsigned long, unsigned long);
    unsigned long(__stdcall* Idle)(BINKIO*);
    void(__stdcall* Close)(BINKIO*);
    void* slot18;
    BINK* bink;
    unsigned long ReadError;
    unsigned long DoingARead;
    unsigned long BytesRead;
    unsigned long Working;
    unsigned long TotalTime;
    unsigned long ForegroundTime;
    unsigned long IdleTime;
    unsigned long ThreadTime;
    unsigned long BufSize;
    unsigned long BufHighUsed;
    unsigned long CurBufSize;
    unsigned long CurBufUsed;
    unsigned long field50;
    unsigned char iodata[160];
    void(__stdcall* suspend_callback)(BINKIO*);
    long(__stdcall* try_suspend_callback)(BINKIO*);
    void(__stdcall* resume_callback)(BINKIO*);
    void(__stdcall* idle_on_callback)(BINKIO*);
    unsigned long callback_control[16];
};

#endif
 
 
class RedBitmapElement;

class RedMoviePlayer : public RedMoviePlayerBase, public RedMoviePlayerRenderer
{
public:

    // FUNCTION: SWBF2 0x006f41d0
    RedMoviePlayer();

    // FUNCTION: SWBF2 0x006f4270
    virtual ~RedMoviePlayer();

    bool Open(const char* const pFileName, unsigned int uiNumTextures, int iHeap,
              int iUnused, unsigned int uiArg4, unsigned int uiBufferSize,
              unsigned int uiReadSize, unsigned int uiNumBitmaps);
    void Close();
    void Flush();
    void EndFrame();
    void Update();
    float GetPlaybackTime() const;
    void Render(const RedTexture* const pTexture, int iX, int iY,
                unsigned int uiWidth, unsigned int uiHeight, unsigned char ucFlags);
    void SeekSegment(const RedMoviePlayerBase::Segment* const pSegment, bool bFlag);
    void SetGain(float fGain);

    virtual RedTexture* AllocTexture(const unsigned int uiNameHash,
                                     const unsigned int uiWidth,
                                     const unsigned int uiHeight,
                                     const unsigned int uiFormat,
                                     const unsigned int uiIndex,
                                     const unsigned char ucFlags);
    virtual void FreeTexture(RedTexture* pTexture, const unsigned int uiIndex,
                             const unsigned char ucFlags);

    bool CreateBinkDecoder();
    void DestroyBinkDecoder(bool bForce);

#ifdef OPENSWBF2_FFMPEG
    static int __cdecl MovieRead(void* pUser, unsigned char* pDest, int iSize);
    void StartClock();
    double GetClock() const;
    void UpdateSound();
    void FillSound();
    void CloseSound();
#else
    static BINKFILE* __cdecl BinkFile(BINKIO* bio);
    static long __stdcall BinkFileOpen(BINKIO* bio, const char* pName, unsigned long uiFlags);
    static unsigned long __stdcall BinkFileReadHeader(BINKIO* bio, long iOffset, void* pDest,
                                                      unsigned long uiSize);
    static unsigned long __stdcall BinkFileReadFrame(BINKIO* bio, unsigned long uiFrame,
                                                     long iOffset, void* pDest,
                                                     unsigned long uiSize);
    static unsigned long __stdcall BinkFileGetBufferSize(BINKIO* bio, unsigned long uiSize);
    static void __stdcall BinkFileSetInfo(BINKIO* bio, void* pBuffer, unsigned long uiSize,
                                          unsigned long uiFileSize, unsigned long uiSimulate);
    static unsigned long __stdcall BinkFileIdle(BINKIO* bio);
    static void __stdcall BinkFileClose(BINKIO* bio);
    static void __cdecl BinkIOSuspend(BINKIO* bio);
    static bool __cdecl BinkIOSuspendTry(BINKIO* bio);
    static void __cdecl BinkIOResume(BINKIO* bio);
    static void __cdecl BinkIOIdleRun(BINKIO* bio);
#endif

    static RedMoviePlayer* sMoviePlayer;

    static unsigned int sReadSize;

#ifdef OPENSWBF2_FFMPEG
    FFMovieDecoder mDecoder;
#else
    BINK* mBink;
#endif

    unsigned char mFlags;

#ifdef OPENSWBF2_FFMPEG
    // Streaming buffer for the movie's audio. Positions count bytes since the first frame:
    // mSoundPlayed is where the play cursor has got to (the movie clock), mSoundWritten the end
    // of what is in the buffer, mSoundQueued how much PCM the decoder has handed over.
    IDirectSoundBuffer* mSoundBuffer;
    unsigned int mSoundSize;
    unsigned int mSoundBlock;
    unsigned int mSoundBytesPerSec;
    unsigned int mSoundPlayPos;
    unsigned __int64 mSoundPlayed;
    unsigned __int64 mSoundWritten;
    unsigned __int64 mSoundQueued;
    __int64 mSoundTime;
    __int64 mClockStart;
    long mSoundVolume;
    float mGain;
    bool mClockRunning;
#endif
};

 
