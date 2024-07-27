#pragma once

#include <array>
#include <deque>

#include "../ConfigStructs.h"

#include "../SDK/NetworkChannel.h"
#include "../SDK/matrix3x4.h"
#include "../SDK/ModelInfo.h"
#include "../SDK/Vector.h"

enum class FrameStage;
struct UserCmd;

namespace Backtrack
{
    struct Record {
        Vector origin;
        float simulationTime;
        matrix3x4 matrix[256];
        StudioHdr* hdr;
        Vector max;
        Vector mins;
        Vector head;
    };

    extern std::deque<Record> records[65];

    void run(UserCmd*) noexcept;

    void addLatencyToNetwork(NetworkChannel*, float) noexcept;
    void updateIncomingSequences() noexcept;

 //   int timeToTicks(float time) noexcept;

    void ClearIncomingSequences();


    float getLerp() noexcept;

    void update(FrameStage stage) noexcept;


    struct incomingSequence {
        int inreliablestate;
        int sequencenr;
        float servertime;
        int   m_seq;
        float curtime;
    };
    incomingSequence* last_record{};
    incomingSequence* old_record{};
    struct backtrackData
    {
        float simtime;
        Vector hitboxPos;
    };

    int backtrack_ticks;


    bool valid(float simtime) noexcept;
    void init() noexcept;
    float getMaxUnlag() noexcept;
}

#pragma once


#ifdef _DEBUG 
#define ENABLE_VMP 0
#endif

#if ENABLE_VMP

#include "VMProtectSDK.h"

#define VMP_M(name) VMProtectBeginMutation(name);
#define VMP_V(name) VMProtectBeginVirtualization(name);
#define VMP_U(name) VMProtectBeginUltra(name);
#define VMP_END() VMProtectEnd();

#else
#define VMP_M(name) 
#define VMP_V(name) 
#define VMP_U(name) 
#define VMP_END() 
#endif