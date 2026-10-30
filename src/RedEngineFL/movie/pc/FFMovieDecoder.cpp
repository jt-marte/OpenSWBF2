#ifdef OPENSWBF2_FFMPEG

#include "FFMovieDecoder.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

// AVIO's read-ahead; the bink demuxer asks for each frame's packet whole on top of this
static const int kIOBufferSize = 0x8000;

// binkw32's YUV to RGB, fitted from BinkCopyToBuffer output: studio-range luma, and V to R, U to
// B, U to G and V to G weights of 1.4, 1.75, 0.35 and 0.7 times the 1.164 luma scale (16.16,
// in sws_setColorspaceDetails order). Red and blue then match binkw32 to a level; FFmpeg's
// BT.601 table is up to 5 levels off in green, this leaves a steady 1-2.
static const int kBinkYuvToRgb[4] = { 106797, 133497, 26699, 53399 };

FFMovieDecoder::FFMovieDecoder()
{
    mRead = 0;
    mSeek = 0;
    mUser = 0;
    mSize = -1;
    mPos = 0;
    memset(mHeader, 0, sizeof(mHeader));

    mIO = 0;
    mFormat = 0;
    mVideoCodec = 0;
    mAudioCodec = 0;
    mVideoFrame = 0;
    mAudioFrame = 0;
    mPacket = 0;
    mSws = 0;
    mSwr = 0;
    mVideoStream = -1;
    mAudioStream = -1;
    mSwsFormat = -1;
    mSwsRange = -1;

    mWidth = 0;
    mHeight = 0;
    mFrames = 0;
    mFrameRate = 0;
    mFrameRateDiv = 0;
    mFramesDecoded = 0;
    mAudioRate = 0;
    mAudioChannels = 0;

    mAudioBuffer = 0;
    mAudioSize = 0;
    mAudioStart = 0;
    mAudioEnd = 0;

    mErrorCount = 0;
    mLastError[0] = 0;
    mHaveFrame = false;
    mDemuxEnd = false;
    mEnd = false;
}

FFMovieDecoder::~FFMovieDecoder()
{
    Close();
}

int FFMovieDecoder::IORead(void* pOpaque, unsigned char* pBuf, int iSize)
{
    FFMovieDecoder* pThis = (FFMovieDecoder*)pOpaque;
    if (pThis->mSize >= 0)
    {
        const __int64 iLeft = pThis->mSize - pThis->mPos;
        if (iLeft <= 0)
        {
            return AVERROR_EOF;
        }
        if (iSize > iLeft)
        {
            iSize = (int)iLeft;
        }
    }

    const int iRead = pThis->mRead(pThis->mUser, pBuf, iSize);
    if (iRead == 0)
    {
        return AVERROR_EOF;
    }
    if (iRead < 0)
    {
        return AVERROR(EIO);
    }

    // Without a size from the caller, take it from the header ("BIK" + version, then the file
    // size less 8) so the reads stop where the movie does
    const int iHeaderSize = (int)sizeof(pThis->mHeader);
    if (pThis->mPos < iHeaderSize)
    {
        const int iHeaderPos = (int)pThis->mPos;
        const int iCopy = iHeaderSize - iHeaderPos < iRead ? iHeaderSize - iHeaderPos : iRead;
        memcpy(pThis->mHeader + iHeaderPos, pBuf, iCopy);
        if (pThis->mSize < 0 && iHeaderPos + iCopy == iHeaderSize &&
            memcmp(pThis->mHeader, "BIK", 3) == 0)
        {
            const unsigned char* pHeader = pThis->mHeader;
            pThis->mSize = (__int64)(pHeader[4] | (pHeader[5] << 8) | (pHeader[6] << 16) |
                                     ((unsigned int)pHeader[7] << 24)) + 8;
        }
    }
    pThis->mPos += iRead;
    return iRead;
}

__int64 FFMovieDecoder::IOSeek(void* pOpaque, __int64 iOffset, int iWhence)
{
    FFMovieDecoder* pThis = (FFMovieDecoder*)pOpaque;
    iWhence &= ~AVSEEK_FORCE;
    if (iWhence == AVSEEK_SIZE)
    {
        return pThis->mSize >= 0 ? pThis->mSize : AVERROR(ENOSYS);
    }

    const __int64 iPos = pThis->mSeek(pThis->mUser, iOffset, iWhence);
    if (iPos < 0)
    {
        return AVERROR(EIO);
    }
    pThis->mPos = iPos;
    return iPos;
}

bool FFMovieDecoder::Open(ReadFunc pfnRead, SeekFunc pfnSeek, void* pUser, __int64 iSize)
{
    Close();
    mErrorCount = 0;
    mLastError[0] = 0;

    mRead = pfnRead;
    mSeek = pfnSeek;
    mUser = pUser;
    mSize = iSize;
    mPos = 0;

    unsigned char* pBuffer = (unsigned char*)av_malloc(kIOBufferSize);
    if (pBuffer)
    {
        mIO = avio_alloc_context(pBuffer, kIOBufferSize, 0, this, IORead, 0,
                                 pfnSeek ? IOSeek : 0);
        if (!mIO)
        {
            av_free(pBuffer);
        }
    }
    AVFormatContext* pFormat = mIO ? avformat_alloc_context() : 0;
    if (!pFormat)
    {
        SetError("open", AVERROR(ENOMEM));
        Close();
        return false;
    }

    // Name the demuxer rather than probe for it: probing reads ahead, and the movies are all Bink
    pFormat->pb = mIO;
    int iError = avformat_open_input(&pFormat, "", av_find_input_format("bink"), 0);
    if (iError < 0)
    {
        // avformat_open_input frees the context on failure, but never a caller's AVIOContext
        SetError("avformat_open_input", iError);
        Close();
        return false;
    }
    mFormat = pFormat;

    // Bink plays the first audio track unless told otherwise; drop the rest at the demuxer
    for (unsigned int i = 0; i < mFormat->nb_streams; ++i)
    {
        AVStream* pStream = mFormat->streams[i];
        if (pStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && mVideoStream < 0)
        {
            mVideoStream = (int)i;
        }
        else if (pStream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && mAudioStream < 0)
        {
            mAudioStream = (int)i;
        }
        else
        {
            pStream->discard = AVDISCARD_ALL;
        }
    }

    if (mVideoStream < 0)
    {
        SetError("open", AVERROR_STREAM_NOT_FOUND);
        Close();
        return false;
    }
    if (!OpenCodec(mVideoStream, &mVideoCodec))
    {
        Close();
        return false;
    }
    if (mAudioStream >= 0 && !OpenCodec(mAudioStream, &mAudioCodec))
    {
        // Play it silent rather than not at all
        mFormat->streams[mAudioStream]->discard = AVDISCARD_ALL;
        mAudioStream = -1;
    }

    mVideoFrame = av_frame_alloc();
    mAudioFrame = av_frame_alloc();
    mPacket = av_packet_alloc();
    if (!mVideoFrame || !mAudioFrame || !mPacket)
    {
        SetError("open", AVERROR(ENOMEM));
        Close();
        return false;
    }

    const AVStream* pVideo = mFormat->streams[mVideoStream];
    mWidth = (unsigned int)pVideo->codecpar->width;
    mHeight = (unsigned int)pVideo->codecpar->height;
    mFrames = pVideo->duration > 0 ? (unsigned int)pVideo->duration : 0;
    mFrameRate = (unsigned int)pVideo->avg_frame_rate.num;
    mFrameRateDiv = (unsigned int)pVideo->avg_frame_rate.den;
    if (!mFrameRate || !mFrameRateDiv)
    {
        mFrameRate = (unsigned int)pVideo->time_base.den;
        mFrameRateDiv = (unsigned int)pVideo->time_base.num;
    }

    if (mAudioCodec)
    {
        mAudioRate = (unsigned int)mAudioCodec->sample_rate;
        mAudioChannels = mAudioCodec->ch_layout.nb_channels > 1 ? 2 : 1;
    }
    return true;
}

bool FFMovieDecoder::OpenCodec(int iStream, AVCodecContext** ppCodec)
{
    const AVStream* pStream = mFormat->streams[iStream];
    const AVCodec* pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
    if (!pCodec)
    {
        SetError("avcodec_find_decoder", AVERROR_DECODER_NOT_FOUND);
        return false;
    }

    AVCodecContext* pContext = avcodec_alloc_context3(pCodec);
    if (!pContext)
    {
        SetError("avcodec_alloc_context3", AVERROR(ENOMEM));
        return false;
    }
    int iError = avcodec_parameters_to_context(pContext, pStream->codecpar);
    if (iError >= 0)
    {
        pContext->pkt_timebase = pStream->time_base;
        iError = avcodec_open2(pContext, pCodec, 0);
    }
    if (iError < 0)
    {
        SetError("avcodec_open2", iError);
        avcodec_free_context(&pContext);
        return false;
    }
    *ppCodec = pContext;
    return true;
}

void FFMovieDecoder::Close()
{
    if (mFormat)
    {
        avformat_close_input(&mFormat);
    }
    if (mIO)
    {
        // FFmpeg may have swapped the buffer we gave it for a bigger one
        av_freep(&mIO->buffer);
        avio_context_free(&mIO);
    }
    avcodec_free_context(&mVideoCodec);
    avcodec_free_context(&mAudioCodec);
    av_frame_free(&mVideoFrame);
    av_frame_free(&mAudioFrame);
    av_packet_free(&mPacket);
    sws_freeContext(mSws);
    mSws = 0;
    swr_free(&mSwr);
    av_freep(&mAudioBuffer);

    mRead = 0;
    mSeek = 0;
    mUser = 0;
    mSize = -1;
    mPos = 0;
    memset(mHeader, 0, sizeof(mHeader));
    mVideoStream = -1;
    mAudioStream = -1;
    mSwsFormat = -1;
    mSwsRange = -1;
    mWidth = 0;
    mHeight = 0;
    mFrames = 0;
    mFrameRate = 0;
    mFrameRateDiv = 0;
    mFramesDecoded = 0;
    mAudioRate = 0;
    mAudioChannels = 0;
    mAudioSize = 0;
    mAudioStart = 0;
    mAudioEnd = 0;
    mHaveFrame = false;
    mDemuxEnd = false;
    mEnd = false;
}

unsigned int FFMovieDecoder::GetFrameNum() const
{
    return mFramesDecoded < mFrames ? mFramesDecoded + 1 : mFrames;
}

double FFMovieDecoder::GetFrameTime(unsigned int uiFrame) const
{
    if (!mFrameRate)
    {
        return 0.0;
    }
    return (double)uiFrame * (double)mFrameRateDiv / (double)mFrameRate;
}

bool FFMovieDecoder::DecodeFrame()
{
    // avcodec_receive_frame drops the frame it held whatever it returns
    mHaveFrame = false;
    if (!mFormat || mEnd)
    {
        return false;
    }

    for (;;)
    {
        int iError = avcodec_receive_frame(mVideoCodec, mVideoFrame);
        if (iError >= 0)
        {
            ++mFramesDecoded;
            mHaveFrame = true;
            return true;
        }
        if (iError == AVERROR_EOF)
        {
            break;
        }
        if (iError != AVERROR(EAGAIN))
        {
            SetError("video decode", iError);
        }
        if (mDemuxEnd)
        {
            break;
        }

        // Bink stores each frame's audio ahead of its video, so the audio track is always
        // decoded somewhat ahead of the picture
        iError = av_read_frame(mFormat, mPacket);
        if (iError < 0)
        {
            if (iError != AVERROR_EOF)
            {
                SetError("read", iError);
            }
            mDemuxEnd = true;
            avcodec_send_packet(mVideoCodec, 0);
            if (mAudioCodec)
            {
                DecodeAudio(0);
            }
            continue;
        }

        if (mPacket->stream_index == mVideoStream)
        {
            iError = avcodec_send_packet(mVideoCodec, mPacket);
            if (iError < 0)
            {
                SetError("video packet", iError);
            }
        }
        else if (mPacket->stream_index == mAudioStream)
        {
            DecodeAudio(mPacket);
        }
        av_packet_unref(mPacket);
    }

    mEnd = true;
    return false;
}

void FFMovieDecoder::DecodeAudio(const AVPacket* pPacket)
{
    int iError = avcodec_send_packet(mAudioCodec, pPacket);
    if (iError < 0)
    {
        if (iError != AVERROR_EOF)
        {
            SetError("audio packet", iError);
        }
        return;
    }

    // One Bink audio packet can hold many frames: the first carries most of a second
    for (;;)
    {
        iError = avcodec_receive_frame(mAudioCodec, mAudioFrame);
        if (iError < 0)
        {
            if (iError != AVERROR(EAGAIN) && iError != AVERROR_EOF)
            {
                SetError("audio decode", iError);
            }
            return;
        }
        if (!QueueAudio(mAudioFrame))
        {
            return;
        }
    }
}

bool FFMovieDecoder::QueueAudio(const AVFrame* pFrame)
{
    if (!mSwr)
    {
        AVChannelLayout outLayout;
        av_channel_layout_default(&outLayout, (int)mAudioChannels);
        int iError = swr_alloc_set_opts2(&mSwr, &outLayout, AV_SAMPLE_FMT_S16, (int)mAudioRate,
                                         &pFrame->ch_layout, (AVSampleFormat)pFrame->format,
                                         pFrame->sample_rate, 0, 0);
        if (iError >= 0)
        {
            iError = swr_init(mSwr);
        }
        av_channel_layout_uninit(&outLayout);
        if (iError < 0)
        {
            SetError("swr_init", iError);
            swr_free(&mSwr);
            return false;
        }
    }

    const int iMaxSamples = swr_get_out_samples(mSwr, pFrame->nb_samples);
    if (iMaxSamples <= 0)
    {
        return true;
    }

    const unsigned int uiBlock = mAudioChannels * 2;
    const unsigned int uiNeed = (unsigned int)iMaxSamples * uiBlock;
    if (mAudioEnd + uiNeed > mAudioSize)
    {
        // Slide the unread PCM down first and only grow when that is not enough
        if (mAudioStart)
        {
            memmove(mAudioBuffer, mAudioBuffer + mAudioStart, mAudioEnd - mAudioStart);
            mAudioEnd -= mAudioStart;
            mAudioStart = 0;
        }
        if (mAudioEnd + uiNeed > mAudioSize)
        {
            unsigned int uiSize = mAudioSize ? mAudioSize : 0x10000;
            while (uiSize < mAudioEnd + uiNeed)
            {
                uiSize *= 2;
            }
            unsigned char* pBuffer = (unsigned char*)av_realloc(mAudioBuffer, uiSize);
            if (!pBuffer)
            {
                SetError("audio queue", AVERROR(ENOMEM));
                return false;
            }
            mAudioBuffer = pBuffer;
            mAudioSize = uiSize;
        }
    }

    unsigned char* pOut = mAudioBuffer + mAudioEnd;
    const int iSamples = swr_convert(mSwr, &pOut, iMaxSamples,
                                     (const unsigned char* const*)pFrame->extended_data,
                                     pFrame->nb_samples);
    if (iSamples < 0)
    {
        SetError("swr_convert", iSamples);
        return false;
    }
    mAudioEnd += (unsigned int)iSamples * uiBlock;
    return true;
}

unsigned int FFMovieDecoder::ReadAudio(void* pDest, unsigned int uiBytes)
{
    const unsigned int uiAvailable = mAudioEnd - mAudioStart;
    if (uiBytes > uiAvailable)
    {
        uiBytes = uiAvailable;
    }
    if (pDest && uiBytes)
    {
        memcpy(pDest, mAudioBuffer + mAudioStart, uiBytes);
    }
    mAudioStart += uiBytes;
    if (mAudioStart == mAudioEnd)
    {
        mAudioStart = 0;
        mAudioEnd = 0;
    }
    return uiBytes;
}

bool FFMovieDecoder::CopyToBuffer(void* pDest, int iPitch, unsigned int uiHeight)
{
    if (!mHaveFrame || !pDest || uiHeight < (unsigned int)mVideoFrame->height)
    {
        return false;
    }

    const int iWidth = mVideoFrame->width;
    const int iHeight = mVideoFrame->height;
    SwsContext* pSws = sws_getCachedContext(mSws, iWidth, iHeight, (AVPixelFormat)mVideoFrame->format,
                                            iWidth, iHeight, AV_PIX_FMT_BGRA, SWS_BILINEAR, 0, 0, 0);
    if (!pSws)
    {
        SetError("sws_getCachedContext", AVERROR(EINVAL));
        return false;
    }

    // Studio-range frames get the matrix binkw32 uses, so colours match the Win32 build; full
    // range (only Bink 2's 'k' revision) gets plain BT.601
    const int iRange = mVideoFrame->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
    if (pSws != mSws || mSwsFormat != mVideoFrame->format || mSwsRange != iRange)
    {
        const int* pCoefficients = sws_getCoefficients(SWS_CS_ITU601);
        sws_setColorspaceDetails(pSws, iRange ? pCoefficients : kBinkYuvToRgb, iRange, pCoefficients,
                                 1, 0, 1 << 16, 1 << 16);
        mSws = pSws;
        mSwsFormat = mVideoFrame->format;
        mSwsRange = iRange;
    }

    unsigned char* apDest[4] = { (unsigned char*)pDest, 0, 0, 0 };
    const int aiPitch[4] = { iPitch, 0, 0, 0 };
    return sws_scale(mSws, mVideoFrame->data, mVideoFrame->linesize, 0, iHeight, apDest, aiPitch) == iHeight;
}

void FFMovieDecoder::SetError(const char* pWhat, int iError)
{
    char acText[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(iError, acText, sizeof(acText));
    _snprintf(mLastError, sizeof(mLastError) - 1, "%s: %s", pWhat, acText);
    mLastError[sizeof(mLastError) - 1] = 0;
    ++mErrorCount;
}

#endif
