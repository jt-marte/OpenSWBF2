#pragma once

#include <stddef.h>
#include "sndstreamnode.h"
#include "sndstreampacket.h"

namespace Snd
{

class StreamSourceProducer : public StreamNodeSource
{
public:

    StreamSourceProducer();
    virtual ~StreamSourceProducer();

    virtual StreamPacket* GetPacket(unsigned int uiOutput);

    void SetPackets(unsigned int uiPacketSize, StreamPacket* pPackets,
                    unsigned int uiNumPackets);

    void SetPackets(unsigned int uiPacketSize, StreamPacket* pPackets, unsigned int uiNumPackets,
                    int iUnread);

    void Flush();

    StreamPacket* GetReadPacket();
    void PacketComplete(StreamPacket* const pPacket, unsigned int uiBufferUsed,
                        unsigned int uiUnused, bool bFlagBit0, bool bFlagBit1,
                        unsigned int uiPos);

#ifdef _WIN64
    StreamPacket* mPackets; // retail kept this in mProducer[0], which x64 leaves unused
    unsigned int mProducer[5];
#else
    union
    {
        StreamPacket* mPackets; // the same word as mProducer[0], as retail laid it out
        unsigned int mProducer[5];
    };
#endif

    unsigned int GetNumQueuedPackets() const;
};

 
 

class StreamSourceNProducer : public StreamNodeSource
{
public:
    StreamSourceNProducer();
    virtual ~StreamSourceNProducer();
    virtual StreamPacket* GetPacket(unsigned int uiOutput);
    void SetProducers(StreamSourceProducer* pProducers,
                      unsigned int uiNumProducers);

    StreamPacket* GetWritablePacket(unsigned int uiOutput);

    unsigned int CompleteWrittenPacket(StreamPacket* pPacket, unsigned int uiBufferUsed,
                                       unsigned int uiUnused, bool bFlagBit0, bool bFlagBit1,
                                       unsigned int uiPos, unsigned int uiOutput);

    StreamSourceProducer* mProducers;
    unsigned int mNumProducers;
};

 
 
 

}
