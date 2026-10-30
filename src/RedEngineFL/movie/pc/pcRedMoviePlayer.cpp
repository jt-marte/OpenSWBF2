#include <windows.h>
#include <string.h>

#include "pcRedMoviePlayer.h"
#include "RedEngineFL/Graphics/PC/pcRedTexture.h"
#include "RedEngineFL/Memory/RedMemory.h"
#include "SoundFL/source/win/sndengine.h"
#include "PebbleFL/Common/PblMath.h"

void __cdecl PblTraceP(const char* Format, ...);

#ifdef OPENSWBF2_FFMPEG

#include <math.h>

#else

extern "C"
{

    typedef long(__stdcall* BINKIOOPEN)(BINKIO* io, const char* name, unsigned long flags);
    typedef void*(__stdcall* BINKMEMALLOC)(unsigned int bytes);
    typedef void(__stdcall* BINKMEMFREE)(void* ptr);

    int __stdcall BinkSetIOSize(unsigned int iosize);
    BINKIOOPEN __stdcall BinkSetIO(BINKIOOPEN io);
    void __stdcall BinkSetMemory(BINKMEMALLOC a, BINKMEMFREE f);

    typedef void*(__stdcall* BINKSNDSYSOPEN)(unsigned long param);
    void* __stdcall BinkOpenDirectSound(unsigned long param);
    int __stdcall BinkSetSoundSystem(BINKSNDSYSOPEN open, unsigned long param);
    BINK* __stdcall BinkOpen(const char* name, unsigned int flags);
    void __stdcall BinkClose(BINK* bink);
    char* __stdcall BinkGetError();
    int __stdcall BinkWait(BINK* bink);
    int __stdcall BinkDoFrame(BINK* bink);
    void __stdcall BinkNextFrame(BINK* bink);
    int __stdcall BinkCopyToBuffer(BINK* bink, void* dest, int destpitch,
                                   unsigned int destheight, unsigned int destx,
                                   unsigned int desty, unsigned int flags);
    int __stdcall BinkDX9SurfaceType(IDirect3DSurface9* surface);
    void __stdcall BinkSetVolume(BINK* bink, unsigned int trackid, int volume);
}

void* __stdcall local_alloc(unsigned int bytes);
void __stdcall local_free(void* ptr);

// FUNCTION: SWBF2 0x006f4c00
void* __stdcall local_alloc(unsigned int bytes)
{
    return RedAllocFromHeap(__RedCurrHeap, bytes, 0);
}

// FUNCTION: SWBF2 0x006f4c20
void __stdcall local_free(void* ptr)
{
    RedFreeToHeap(__RedCurrHeap, ptr, 0);
}

#endif

// GLOBAL: SWBF2 0x009c8420
RedMoviePlayer* RedMoviePlayer::sMoviePlayer;

// GLOBAL: SWBF2 0x009c8424
unsigned int RedMoviePlayer::sReadSize;

// FUNCTION: SWBF2 0x006f41d0
RedMoviePlayer::RedMoviePlayer()
    : RedMoviePlayerBase(0x800)
{
#ifdef OPENSWBF2_FFMPEG
    mFlags = 0;
    mSoundBuffer = 0;
    mSoundSize = 0;
    mSoundBlock = 0;
    mSoundBytesPerSec = 0;
    mSoundPlayPos = 0;
    mSoundPlayed = 0;
    mSoundWritten = 0;
    mSoundQueued = 0;
    mSoundTime = 0;
    mClockStart = 0;
    mSoundVolume = 0;
    mGain = 1.0f;
    mClockRunning = false;

    sMoviePlayer = this;
#else
    mBink = 0;
    mFlags = 0;

    sMoviePlayer = this;

    BinkSetIOSize(0);
    BinkSetIO(BinkFileOpen);
    BinkSetMemory(local_alloc, local_free);
#endif
}

// FUNCTION: SWBF2 0x006f4270
RedMoviePlayer::~RedMoviePlayer()
{
#ifdef OPENSWBF2_FFMPEG
    CloseSound();
#endif
    sMoviePlayer = 0;
}

// SYNTHETIC: SWBF2 0x006f42e0
// RedMoviePlayer::`scalar deleting destructor'

// FUNCTION: SWBF2 0x006f4310
RedTexture* RedMoviePlayer::AllocTexture(const unsigned int uiNameHash,
                                         const unsigned int uiWidth,
                                         const unsigned int uiHeight,
                                         const unsigned int uiFormat,
                                         const unsigned int uiIndex,
                                         const unsigned char ucFlags)
{
    int iOldHeap = RedSetCurrentHeap(GetInfo()->GetRedHeap());
    RedTexture* pTexture = new RedTexture(uiWidth, uiHeight, D3DFMT_X8R8G8B8, 1, 0,
                                          D3DPOOL_MANAGED, uiNameHash);
    RedSetCurrentHeap(iOldHeap);
    return pTexture;
}

#ifdef OPENSWBF2_FFMPEG

// FFmpeg's side of BinkFileReadFrame: hands over what RedMoviePlayerBase has streamed into the
// circular buffer, running the file reads while it is empty. The buffer only ever holds the
// current movie and FFMovieDecoder stops at the size in its header, so running dry with the
// base already on the next segment means this one was cut short (a new segment was queued
// mid-movie): flag it the way Bink's reader did.
int RedMoviePlayer::MovieRead(void* pUser, unsigned char* pDest, int iSize)
{
    RedMoviePlayer* pPlayer = (RedMoviePlayer*)pUser;
    while (!pPlayer->BufferGetUsed())
    {
        if (pPlayer->IsDiscontinuity())
        {
            pPlayer->mFlags |= 4;
        }
        else if (pPlayer->IsEnd())
        {
            pPlayer->mFlags |= 2;
        }
        if (pPlayer->mFlags & 6)
        {
            return 0;
        }
        pPlayer->RedMoviePlayerBase::Update();
        Sleep(0);
    }
    return (int)pPlayer->BufferReadData(pDest, (unsigned int)iSize, true);
}

// BinkSetVolume took the gain scaled to 32767; DirectSound takes attenuation in 1/100 dB
static long GainToVolume(float fGain)
{
    if (fGain <= 0.0f)
    {
        return DSBVOLUME_MIN;
    }
    if (fGain >= 1.0f)
    {
        return DSBVOLUME_MAX;
    }
    const long lVolume = (long)(2000.0f * log10f(fGain));
    return lVolume < DSBVOLUME_MIN ? DSBVOLUME_MIN : lVolume;
}

// Starts the movie clock as the first frame goes up, with the audio playing from the same
// moment. What BinkOpenDirectSound did: a looping DirectSound buffer on the game's device, fed
// from Update. Without sound the clock runs off the performance counter.
void RedMoviePlayer::StartClock()
{
    LARGE_INTEGER liNow;
    QueryPerformanceCounter(&liNow);
    mClockStart = liNow.QuadPart;
    mClockRunning = true;

    IDirectSound8* pDirectSound = Snd::Engine::smDirectSound;
    const unsigned int uiChannels = mDecoder.GetAudioChannels();
    const unsigned int uiRate = mDecoder.GetAudioRate();
    if (!pDirectSound || !mDecoder.HasAudio() || !uiChannels || !uiRate)
    {
        return;
    }

    WAVEFORMATEX format;
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = (WORD)uiChannels;
    format.nSamplesPerSec = uiRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = (WORD)(uiChannels * 2);
    format.nAvgBytesPerSec = uiRate * format.nBlockAlign;

    // Two seconds: the movies carry about 0.8 s of audio ahead of the picture, and the rest
    // rides out a hitch on silence instead of wrapping round onto old audio
    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = format.nAvgBytesPerSec * 2;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* pBuffer = 0;
    const HRESULT hr = pDirectSound->CreateSoundBuffer(&desc, &pBuffer, 0);
    if (FAILED(hr))
    {
        PblTraceP("Movie sound: CreateSoundBuffer failed (0x%08x)\n", (unsigned int)hr);
        return;
    }

    mSoundBuffer = pBuffer;
    mSoundSize = desc.dwBufferBytes;
    mSoundBlock = format.nBlockAlign;
    mSoundBytesPerSec = format.nAvgBytesPerSec;
    mSoundPlayPos = 0;
    mSoundPlayed = 0;
    mSoundWritten = 0;
    mSoundQueued = 0;
    mSoundTime = liNow.QuadPart;
    mSoundVolume = GainToVolume(mGain);
    mSoundBuffer->SetVolume(mSoundVolume);
    FillSound();
    mSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

// Seconds since the first frame went up, by the audio when there is any, as BinkWait does
double RedMoviePlayer::GetClock() const
{
    if (mSoundBuffer)
    {
        return (double)mSoundPlayed / (double)mSoundBytesPerSec;
    }
    LARGE_INTEGER liNow, liFreq;
    QueryPerformanceCounter(&liNow);
    QueryPerformanceFrequency(&liFreq);
    return (double)(liNow.QuadPart - mClockStart) / (double)liFreq.QuadPart;
}

void RedMoviePlayer::UpdateSound()
{
    if (!mSoundBuffer)
    {
        // Sound is off: the decoded audio has nowhere to go
        if (mClockRunning)
        {
            mDecoder.ReadAudio(0, mDecoder.GetAudioBytes());
        }
        return;
    }

    DWORD dwPlay, dwWrite;
    if (FAILED(mSoundBuffer->GetCurrentPosition(&dwPlay, &dwWrite)))
    {
        return;
    }
    LARGE_INTEGER liNow, liFreq;
    QueryPerformanceCounter(&liNow);
    QueryPerformanceFrequency(&liFreq);

    // The cursor comes round every two seconds; if the game was away for over one, count the
    // laps it made from the wall clock
    unsigned __int64 uiAdvance = (dwPlay + mSoundSize - mSoundPlayPos) % mSoundSize;
    const double dElapsed = (double)(liNow.QuadPart - mSoundTime) / (double)liFreq.QuadPart *
                            (double)mSoundBytesPerSec;
    if (dElapsed > (double)(mSoundSize / 2))
    {
        const double dLaps = (dElapsed - (double)uiAdvance) / (double)mSoundSize + 0.5;
        if (dLaps >= 1.0)
        {
            uiAdvance += (unsigned __int64)dLaps * mSoundSize;
        }
    }
    mSoundPlayed += uiAdvance;
    mSoundPlayPos = dwPlay;
    mSoundTime = liNow.QuadPart;

    // Ran dry: the cursor went on into the silence FillSound leaves. Keep the time it played
    // and carry on from the write cursor; FillSound drops the audio that was skipped.
    unsigned int uiSafe = (dwWrite + mSoundSize - dwPlay) % mSoundSize;
    uiSafe += (mSoundBlock - uiSafe % mSoundBlock) % mSoundBlock;
    if (mSoundWritten < mSoundPlayed + uiSafe)
    {
        mSoundWritten = mSoundPlayed + uiSafe;
    }
    FillSound();
}

// Tops the buffer up from the decoder and zeroes the rest of the free space, so a stall longer
// than the queued audio plays silence rather than old audio
void RedMoviePlayer::FillSound()
{
    // Audio for time the clock has already passed is dropped to keep sound and picture together
    if (mSoundQueued < mSoundWritten)
    {
        mSoundQueued += mDecoder.ReadAudio(0, (unsigned int)(mSoundWritten - mSoundQueued));
    }

    const unsigned int uiFree = mSoundSize - (unsigned int)(mSoundWritten - mSoundPlayed);
    unsigned int uiBytes = 0;
    if (mSoundQueued == mSoundWritten)
    {
        uiBytes = mDecoder.GetAudioBytes();
        if (uiBytes > uiFree)
        {
            uiBytes = uiFree;
        }
        uiBytes -= uiBytes % mSoundBlock;
    }

    void* pPart1;
    void* pPart2;
    DWORD dwPart1, dwPart2;
    if (!uiFree || FAILED(mSoundBuffer->Lock((DWORD)(mSoundWritten % mSoundSize), uiFree,
                                             &pPart1, &dwPart1, &pPart2, &dwPart2, 0)))
    {
        return;
    }
    const unsigned int uiPart1 = uiBytes < dwPart1 ? uiBytes : dwPart1;
    mDecoder.ReadAudio(pPart1, uiPart1);
    memset((unsigned char*)pPart1 + uiPart1, 0, dwPart1 - uiPart1);
    if (pPart2)
    {
        const unsigned int uiPart2 = uiBytes - uiPart1;
        mDecoder.ReadAudio(pPart2, uiPart2);
        memset((unsigned char*)pPart2 + uiPart2, 0, dwPart2 - uiPart2);
    }
    mSoundBuffer->Unlock(pPart1, dwPart1, pPart2, dwPart2);
    mSoundWritten += uiBytes;
    mSoundQueued += uiBytes;
}

void RedMoviePlayer::CloseSound()
{
    if (mSoundBuffer)
    {
        mSoundBuffer->Stop();
        mSoundBuffer->Release();
        mSoundBuffer = 0;
    }
    mClockRunning = false;
}

#else

// FUNCTION: SWBF2 0x006f43a0
BINKFILE* RedMoviePlayer::BinkFile(BINKIO* bio)
{
    return (BINKFILE*)bio->iodata;
}

// FUNCTION: SWBF2 0x006f43b0
void RedMoviePlayer::BinkFileClose(BINKIO* bio)
{
    BinkIOSuspend(bio);
    BinkIOResume(bio);
}

// FUNCTION: SWBF2 0x006f43d0
unsigned long RedMoviePlayer::BinkFileGetBufferSize(BINKIO* bio, unsigned long uiSize)
{
    return (uiSize / sReadSize) * sReadSize;
}

// FUNCTION: SWBF2 0x006f43f0
unsigned long RedMoviePlayer::BinkFileIdle(BINKIO* bio)
{
    if (BinkIOSuspendTry(bio))
    {
        bio->DoingARead = 1;
        bio->DoingARead = 0;
        return 0;
    }
    BinkIOIdleRun(bio);
    return (unsigned long)-1;
}

// FUNCTION: SWBF2 0x006f4430
long RedMoviePlayer::BinkFileOpen(BINKIO* bio, const char* pName, unsigned long uiFlags)
{
    memset(bio, 0, sizeof(BINKIO));
    BINKFILE* pFile = BinkFile(bio);
    pFile->fileHandle = (unsigned long)sMoviePlayer;
    pFile->dontClose = 1;
    pFile->startFile = 0;
    bio->ReadHeader = BinkFileReadHeader;
    bio->ReadFrame = BinkFileReadFrame;
    bio->GetBufferSize = BinkFileGetBufferSize;
    bio->SetInfo = BinkFileSetInfo;
    bio->Idle = BinkFileIdle;
    bio->Close = BinkFileClose;
    return 1;
}

// FUNCTION: SWBF2 0x006f44a0
unsigned long RedMoviePlayer::BinkFileReadFrame(BINKIO* bio, unsigned long uiFrame,
                                                long iOffset, void* pDest,
                                                unsigned long uiSize)
{
    RedMoviePlayer* pPlayer = (RedMoviePlayer*)BinkFile(bio)->fileHandle;
    BinkIOSuspend(bio);
    while (pPlayer->BufferGetUsed() < uiSize)
    {
        if (pPlayer->IsDiscontinuity())
        {
            pPlayer->mFlags |= 4;
        }
        else if (pPlayer->IsEnd())
        {
            pPlayer->mFlags |= 2;
        }
        if (pPlayer->mFlags & 6)
        {
            return 0;
        }
        pPlayer->RedMoviePlayerBase::Update();
        Sleep(0);
    }
    unsigned long uiRead = pPlayer->BufferReadData(pDest, uiSize, true);
    BinkIOResume(bio);
    return uiRead;
}

// FUNCTION: SWBF2 0x006f4550
unsigned long RedMoviePlayer::BinkFileReadHeader(BINKIO* bio, long iOffset, void* pDest,
                                                 unsigned long uiSize)
{
    unsigned long uiRead = 0;
    RedMoviePlayer* pPlayer = (RedMoviePlayer*)BinkFile(bio)->fileHandle;
    BinkIOSuspend(bio);
    if (pPlayer->BufferGetUsed() >= uiSize)
    {
        uiRead = pPlayer->BufferReadData(pDest, uiSize, true);
    }
    BinkIOResume(bio);
    return uiRead;
}

// FUNCTION: SWBF2 0x006f45a0
void RedMoviePlayer::BinkFileSetInfo(BINKIO* bio, void* pBuffer, unsigned long uiSize,
                                     unsigned long uiFileSize, unsigned long uiSimulate)
{
    BINKFILE* pFile = BinkFile(bio);
    BinkIOSuspend(bio);
    pFile->buffer = (unsigned char*)pBuffer;
    pFile->bufPos = (unsigned char*)pBuffer;
    pFile->bufBack = (unsigned char*)pBuffer;
    pFile->bufEnd = (unsigned char*)pBuffer;
    pFile->fileSize = uiFileSize;
    pFile->bufEmpty = uiSize;
    pFile->simulate = uiSimulate;
    bio->BufSize = uiSize;
    bio->CurBufUsed = 0;
    BinkIOResume(bio);
}

// FUNCTION: SWBF2 0x006f45f0
void RedMoviePlayer::BinkIOIdleRun(BINKIO* bio)
{
    if (bio->idle_on_callback)
    {
        bio->idle_on_callback(bio);
    }
}

// FUNCTION: SWBF2 0x006f4610
void RedMoviePlayer::BinkIOResume(BINKIO* bio)
{
    if (bio->resume_callback)
    {
        bio->resume_callback(bio);
    }
}

// FUNCTION: SWBF2 0x006f4630
void RedMoviePlayer::BinkIOSuspend(BINKIO* bio)
{
    if (bio->suspend_callback)
    {
        bio->suspend_callback(bio);
    }
}

// FUNCTION: SWBF2 0x006f4650
bool RedMoviePlayer::BinkIOSuspendTry(BINKIO* bio)
{
    return bio->try_suspend_callback && bio->try_suspend_callback(bio) != 0;
}

#endif

// FUNCTION: SWBF2 0x006f4670
void RedMoviePlayer::Close()
{
    DestroyBinkDecoder(true);
    CloseRenderer(GetInfo()->GetRedHeap());
    RedFreeToHeap(GetInfo()->GetRedHeap(), BufferGetBuffer(), 0);
    RedMoviePlayerBase::Close();
}

// FUNCTION: SWBF2 0x006f46c0
bool RedMoviePlayer::CreateBinkDecoder()
{
#ifdef OPENSWBF2_FFMPEG
    return CreateTextures(mDecoder.GetWidth(), mDecoder.GetHeight(), 0x20, 3);
#else
    return CreateTextures(mBink->Width, mBink->Height, 0x20, 3);
#endif
}

// FUNCTION: SWBF2 0x006f46e0
void RedMoviePlayer::DestroyBinkDecoder(bool bForce)
{
#ifdef OPENSWBF2_FFMPEG
    if (mDecoder.IsOpen())
    {
        CloseSound();
        mDecoder.Close();
        DestroyTextures(bForce, 3);
    }
#else
    if (mBink)
    {
        BinkClose(mBink);
        mBink = 0;
        DestroyTextures(bForce, 3);
    }
#endif
}

void PumpWaitingMessages();

// FUNCTION: SWBF2 0x006f4720
void RedMoviePlayer::EndFrame()
{
    RedMoviePlayerRenderer::EndFrame();
    PumpWaitingMessages();
}

// FUNCTION: SWBF2 0x006f4730
void RedMoviePlayer::Flush()
{
    RedMoviePlayerBase::Flush();
    DestroyBinkDecoder(false);
}

// FUNCTION: SWBF2 0x006f4750
void RedMoviePlayer::FreeTexture(RedTexture* pTexture, const unsigned int uiIndex,
                                 const unsigned char ucFlags)
{
    int iOldHeap = RedSetCurrentHeap(GetInfo()->GetRedHeap());
    delete pTexture;
    RedSetCurrentHeap(iOldHeap);
}

// FUNCTION: SWBF2 0x006f47a0
float RedMoviePlayer::GetPlaybackTime() const
{
#ifdef OPENSWBF2_FFMPEG
    if (!mDecoder.IsOpen() || !mDecoder.GetFrameRate())
    {
        return 0.0f;
    }
    return (float)mDecoder.GetFrameRateDiv() * (float)mDecoder.GetFrameNum() /
           (float)mDecoder.GetFrameRate();
#else
    if (!mBink)
    {
        return 0.0f;
    }
    return (float)mBink->FrameRateDiv * (float)mBink->FrameNum / (float)mBink->FrameRate;
#endif
}

// FUNCTION: SWBF2 0x006f4820
bool RedMoviePlayer::Open(const char* const pFileName, unsigned int uiNumTextures, int iHeap,
                          int iUnused, unsigned int uiArg4, unsigned int uiBufferSize,
                          unsigned int uiReadSize, unsigned int uiNumBitmaps)
{
    char* pBlock = (char*)RedAllocFromHeap(iHeap, uiBufferSize + 4 * (uiNumBitmaps + 4 * uiNumTextures), 0);
    if (pBlock)
    {
        if (OpenRenderer(iHeap, (RedBitmapElement**)(pBlock + 16 * uiNumTextures + uiBufferSize),
                         uiNumBitmaps))
        {
            if (RedMoviePlayerBase::Open(pFileName, (RedTexture**)(pBlock + uiBufferSize),
                                         uiNumTextures, iHeap, uiArg4, pBlock, uiBufferSize,
                                         uiReadSize, 0xffffffff))
            {
                sReadSize = uiReadSize;
#ifndef OPENSWBF2_FFMPEG
                BinkSetSoundSystem(BinkOpenDirectSound,
                                   (unsigned long)Snd::Engine::smDirectSound);
#endif
                return true;
            }
            CloseRenderer(iHeap);
        }
        RedFreeToHeap(iHeap, pBlock, 0);
    }
    return false;
}

// FUNCTION: SWBF2 0x006f48e0
void RedMoviePlayer::Render(const RedTexture* const pTexture, int iX, int iY,
                            unsigned int uiWidth, unsigned int uiHeight, unsigned char ucFlags)
{
    RedMoviePlayerRenderer::Render(pTexture, iX, iY, uiWidth, uiHeight, ucFlags);
}

// FUNCTION: SWBF2 0x006f48f0
void RedMoviePlayer::SeekSegment(const RedMoviePlayerBase::Segment* const pSegment, bool bFlag)
{
    RedMoviePlayerBase::SeekSegment(pSegment, bFlag);
}

// FUNCTION: SWBF2 0x006f4900
void RedMoviePlayer::SetGain(float fGain)
{
#ifdef OPENSWBF2_FFMPEG
    // Kept for the next movie's buffer too; SetVolume only when the level actually moves
    mGain = fGain;
    if (mSoundBuffer)
    {
        const long lVolume = GainToVolume(fGain);
        if (lVolume != mSoundVolume)
        {
            mSoundBuffer->SetVolume(lVolume);
            mSoundVolume = lVolume;
        }
    }
#else
    if (mBink)
    {
        for (unsigned int i = 0; i < (unsigned int)mBink->NumTracks; ++i)
        {
            BinkSetVolume(mBink, i, (int)(fGain * 32767.0f));
        }
    }
#endif
}

// GLOBAL: SWBF2 0x009c8430
static bool sOpeningBinkDecoder;

// FUNCTION: SWBF2 0x006f4950
void RedMoviePlayer::Update()
{
    switch (GetDesiredPlayState())
    {
    case playState_stopped:
        Disable();
        SetCurrentPlayState(playState_stopped);
        DestroyBinkDecoder(false);
        Flush();
        break;
    case playState_paused:
        SetCurrentPlayState(playState_pausing);
        break;
    case playState_playing:
        SetCurrentPlayState(playState_playing);
        break;
    }

    PlayState eState = GetPlayState();
    if (eState < playState_pausing)
    {
        return;
    }
    if (eState > playState_paused)
    {
        if (eState != playState_playing)
        {
            return;
        }

#ifdef OPENSWBF2_FFMPEG
        if (GetNumReallocTextures() && mDecoder.IsOpen())
        {
            ReallocTextures(mDecoder.GetWidth(), mDecoder.GetHeight(), 0x20, 3);
        }
#else
        if (GetNumReallocTextures() && mBink)
        {
            ReallocTextures(mBink->Width, mBink->Height, 0x20, 3);
        }
#endif

        if (mFlags & 6)
        {
            if (!GetNumUsedTextures())
            {
                bool bEnd = (mFlags & 2) != 0;
                DestroyBinkDecoder(false);
                if (!GetLooping())
                {
                    Stop();
                }
                Notify(bEnd ? notifyType_End : notifyType_Discontinuity);
                mFlags &= ~7;
                Update();
                mFlags |= 1;
            }
        }
#ifdef OPENSWBF2_FFMPEG
        else if (mDecoder.IsOpen() && (mFlags & 1))
        {
            UpdateSound();

            // BinkWait: frame N goes up once the clock reaches N / fps. Two frames behind (a
            // hitch, or the game running slower than the movie), decode on to the frame that is
            // due rather than show the late ones, as Bink skips; every frame still has to be
            // decoded, only the copies are saved. One frame late is just update jitter.
            RedTexture* pTexture = GetTexture();
            if (pTexture && (!mClockRunning ||
                             GetClock() >= mDecoder.GetFrameTime(mDecoder.GetFramesDecoded())))
            {
                bool bFrame = mDecoder.DecodeFrame();
                for (int i = 0; bFrame && mClockRunning && i < 8 &&
                                mDecoder.GetFramesDecoded() < mDecoder.GetFrames() &&
                                GetClock() >= mDecoder.GetFrameTime(mDecoder.GetFramesDecoded() + 1);
                     ++i)
                {
                    bFrame = mDecoder.DecodeFrame();
                }

                // A frame decoded as MovieRead found the movie cut short may be torn; drop it
                if (bFrame && !(mFlags & 6))
                {
                    IDirect3DSurface9* pSurface;
                    ((IDirect3DTexture9*)pTexture->m_pTexture)->GetSurfaceLevel(0, &pSurface);
                    D3DLOCKED_RECT rect;
                    pSurface->LockRect(&rect, 0, 0);
                    if (mDecoder.CopyToBuffer(rect.pBits, rect.Pitch, mDecoder.GetHeight()))
                    {
                        TextureComplete(pTexture);
                    }
                    pSurface->UnlockRect();
                    pSurface->Release();
                    if (!mClockRunning)
                    {
                        StartClock();
                    }
                }
                if (!bFrame || mDecoder.GetFramesDecoded() >= mDecoder.GetFrames())
                {
                    mFlags |= IsDiscontinuity() ? 4 : 2;
                }
            }
        }
#else
        else if (mBink && (mFlags & 1) && !BinkWait(mBink))
        {
            RedTexture* pTexture = GetTexture();
            if (pTexture && !BinkDoFrame(mBink))
            {
                IDirect3DSurface9* pSurface;
                ((IDirect3DTexture9*)pTexture->m_pTexture)->GetSurfaceLevel(0, &pSurface);
                D3DLOCKED_RECT rect;
                pSurface->LockRect(&rect, 0, 0);
                if (!BinkCopyToBuffer(mBink, rect.pBits, rect.Pitch, mBink->Height, 0, 0,
                                      BinkDX9SurfaceType(pSurface) | 0x80000000))
                {
                    TextureComplete(pTexture);
                }
                pSurface->UnlockRect();
                pSurface->Release();
                if (mBink->FrameNum >= mBink->Frames)
                {
                    mFlags |= IsDiscontinuity() ? 4 : 2;
                }
                else
                {
                    BinkNextFrame(mBink);
                }
            }
        }
#endif
    }

    if (GetPlayState() == playState_pausing && GetReadBufferFull())
    {
        SetCurrentPlayState(playState_paused);
    }

    RedMoviePlayerBase::Update();

#ifdef OPENSWBF2_FFMPEG
    if (!mDecoder.IsOpen() && GetReadBufferFull() && !sOpeningBinkDecoder && !(mFlags & 6))
    {
        sOpeningBinkDecoder = true;
        if (IsEnd() || IsDiscontinuity())
        {
            ResetStart();
        }
        // The size comes from the movie's own header: MovieRead cannot tell where it ends
        const bool bOpen = mDecoder.Open(MovieRead, 0, this, -1);
        if (!bOpen)
        {
            // Bink went on to dereference the null BINK here; end the segment instead
            PblTraceP("FFmpeg movie error: %s\n", mDecoder.GetLastError());
            mFlags |= 2;
        }
        mFlags |= 1;
        sOpeningBinkDecoder = false;
        if (bOpen)
        {
            CreateBinkDecoder();
        }
    }
#else
    if (!mBink && GetReadBufferFull() && !sOpeningBinkDecoder)
    {
        sOpeningBinkDecoder = true;
        if (IsEnd() || IsDiscontinuity())
        {
            ResetStart();
        }
        BinkSetIO(BinkFileOpen);
        mBink = BinkOpen("", 0x0B000000);
        if (!mBink)
        {
            PblTraceP("Bink error: %s\n", BinkGetError());
        }
        mFlags |= 1;
        sOpeningBinkDecoder = false;
        CreateBinkDecoder();
    }
#endif
}

// SYNTHETIC: SWBF2 0x00409120
// `dynamic initializer for 'PblMath::fOneOverHalfPi''

// SYNTHETIC: SWBF2 0x00409140
// `dynamic initializer for 'PblMath::fOneOverTwoPi''
