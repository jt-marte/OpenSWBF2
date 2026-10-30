#pragma once

// x64 has no binkw32, so the x64 build decodes the game's Bink movies with FFmpeg. This core
// knows nothing about the engine: the caller feeds it the bytes of one movie through a read
// callback and gets frames in the layout BinkCopyToBuffer writes for an X8R8G8B8 texture, and the
// first audio track as interleaved 16-bit PCM. pcRedMoviePlayer drives it; the standalone movie
// test drives it straight from the .mvs files.

#ifdef OPENSWBF2_FFMPEG

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVIOContext;
struct AVPacket;
struct SwrContext;
struct SwsContext;

class FFMovieDecoder
{
public:

    // Copies up to iSize bytes of the movie to pDest and returns how many, 0 at the end of the
    // movie or negative on a read error. Reads always follow on from the previous one.
    typedef int(__cdecl* ReadFunc)(void* pUser, unsigned char* pDest, int iSize);

    // Optional. iWhence is SEEK_SET, SEEK_CUR or SEEK_END; returns the new position or negative
    // on failure. Without one the movie is read strictly forwards, which is all Bink needs.
    typedef __int64(__cdecl* SeekFunc)(void* pUser, __int64 iOffset, int iWhence);

    FFMovieDecoder();
    ~FFMovieDecoder();

    // iSize is the length of the movie in bytes, or -1 to take it from the Bink header. Reads
    // never go past it, so a stream that cannot tell where the movie ends is never over-read.
    bool Open(ReadFunc pfnRead, SeekFunc pfnSeek, void* pUser, __int64 iSize);
    void Close();
    bool IsOpen() const { return mFormat != 0; }

    unsigned int GetWidth() const { return mWidth; }
    unsigned int GetHeight() const { return mHeight; }

    // Frame count and rate from the header, rate = GetFrameRate() / GetFrameRateDiv() as in BINK
    unsigned int GetFrames() const { return mFrames; }
    unsigned int GetFrameRate() const { return mFrameRate; }
    unsigned int GetFrameRateDiv() const { return mFrameRateDiv; }

    // Frames decoded so far; the current frame is GetFramesDecoded() - 1
    unsigned int GetFramesDecoded() const { return mFramesDecoded; }

    // BINK::FrameNum: the 1-based frame BinkDoFrame decodes next, held at the last frame once
    // every frame is out
    unsigned int GetFrameNum() const;

    // Seconds from the start of the movie at which 0-based frame uiFrame is due
    double GetFrameTime(unsigned int uiFrame) const;

    bool HasAudio() const { return mAudioCodec != 0; }
    unsigned int GetAudioRate() const { return mAudioRate; }
    unsigned int GetAudioChannels() const { return mAudioChannels; }

    // Decodes the next video frame. Audio met on the way is decoded and queued for ReadAudio.
    // Returns false once the movie has no more frames or the stream is broken; IsEnd() says
    // which. Skipping a late frame is just not copying it.
    bool DecodeFrame();
    bool IsEnd() const { return mEnd; }

    // Writes the current frame to pDest as 32-bit pixels, B G R A in memory (the
    // D3DFMT_X8R8G8B8 layout BinkCopyToBuffer writes), rows top-down iPitch bytes apart
    bool CopyToBuffer(void* pDest, int iPitch, unsigned int uiHeight);

    // Queued PCM, interleaved signed 16-bit at GetAudioRate() and GetAudioChannels()
    unsigned int GetAudioBytes() const { return mAudioEnd - mAudioStart; }

    // Takes up to uiBytes of queued PCM; a null pDest drops them
    unsigned int ReadAudio(void* pDest, unsigned int uiBytes);

    // Decode errors so far (bad packets the decoders rejected) and the last error's text
    unsigned int GetErrorCount() const { return mErrorCount; }
    const char* GetLastError() const { return mLastError; }

private:

    static int __cdecl IORead(void* pOpaque, unsigned char* pBuf, int iSize);
    static __int64 __cdecl IOSeek(void* pOpaque, __int64 iOffset, int iWhence);

    bool OpenCodec(int iStream, AVCodecContext** ppCodec);
    void DecodeAudio(const AVPacket* pPacket);
    bool QueueAudio(const AVFrame* pFrame);
    void SetError(const char* pWhat, int iError);

    ReadFunc mRead;
    SeekFunc mSeek;
    void* mUser;
    __int64 mSize;
    __int64 mPos;
    unsigned char mHeader[8];

    AVIOContext* mIO;
    AVFormatContext* mFormat;
    AVCodecContext* mVideoCodec;
    AVCodecContext* mAudioCodec;
    AVFrame* mVideoFrame;
    AVFrame* mAudioFrame;
    AVPacket* mPacket;
    SwsContext* mSws;
    SwrContext* mSwr;
    int mVideoStream;
    int mAudioStream;
    int mSwsFormat;
    int mSwsRange;

    unsigned int mWidth;
    unsigned int mHeight;
    unsigned int mFrames;
    unsigned int mFrameRate;
    unsigned int mFrameRateDiv;
    unsigned int mFramesDecoded;
    unsigned int mAudioRate;
    unsigned int mAudioChannels;

    unsigned char* mAudioBuffer;
    unsigned int mAudioSize;
    unsigned int mAudioStart;
    unsigned int mAudioEnd;

    unsigned int mErrorCount;
    char mLastError[160];
    bool mHaveFrame;
    bool mDemuxEnd;
    bool mEnd;
};

#endif
