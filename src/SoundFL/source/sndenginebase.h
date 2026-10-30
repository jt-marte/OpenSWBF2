#pragma once

#include "sndallocbitmap.h"
#include "sndallocator.h"

#include "win/sndlistener.h"
#include "sndvoicevirtualmanager.h"

class PblFileChunk;

namespace Snd
{

class Stream;
class Voice;

class EngineBase
{
public:

    enum SpeakerConfig
    {
        speakerConfig_Invalid = 0,
        speakerConfig_Mono = 1,
        speakerConfig_Headphones = 2,
        speakerConfig_Stereo = 3,
        speakerConfig_AnalogSurround = 4,
        speakerConfig_Quadraphonic = 5,
        speakerConfig_5point1 = 6,
        speakerConfig_7point1 = 7,
        speakerConfig_max = 8
    };

    struct CallbackOverride
    {
        unsigned int chunkID;
        void (__cdecl* callback)(PblFileChunk& chunk, char const* pcName, void* pData);
        void* callbackData;
    };

    static bool Open();

    static void SetSpeakerConfig(SpeakerConfig eConfig);

    static VoiceVirtualManager* GetVoiceVirtualManager(bool);

    static Listener* GetFreeListener();

    static bool IsOpen();

    static bool LoadConfiguration(PblFileChunk& chunk, char const* pcName,
                                  bool bLoadBanks, CallbackOverride const* pOverrides,
                                  unsigned int uiNumOverrides);

    static void Close();

    static void DestroyAll();

    static Stream* GetOpenStream(unsigned int uiHashID);
    static Stream* GetFreeStream();
    static Stream* GetStream(unsigned int uiIndex);
    static float GetTimeElapsed();
    static void Update(float fDeltaTime, bool b);

    static Stream* smStreams;
    static Voice* smVoices;
    static bool smOpened;
    static const unsigned int kNumVoices = 32;
    static const unsigned int kNumStreams = 6;
    static unsigned char smVoiceStorage[]; // kNumVoices Voices
    static unsigned char smStreamStorage[]; // kNumStreams Streams

    static float smTimeElapsed;

    static SpeakerConfig smSpeakerConfig;

    static VoiceVirtualManager smVoiceVirtualManager;

    static MemoryBitmap smSoundRAMAllocator;

    static Listener smListeners[4];
};

 

}
