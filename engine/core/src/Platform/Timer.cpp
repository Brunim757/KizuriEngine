#include "kizuri/core/platform/Timer.h"

#ifndef _WIN32
#error "KizuriCore Timer targets the Windows platform."
#endif

#include <windows.h>

namespace kizuri::core {

uint64_t FrameTimer::NowTicks() noexcept
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
}

double FrameTimer::TicksToSeconds(uint64_t ticks) noexcept
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return static_cast<double>(ticks) / static_cast<double>(frequency.QuadPart);
}

void FrameTimer::Reset()
{
    uint64_t now = NowTicks();
    baseTick_ = now;
    lastTick_ = now;
    elapsedSeconds_ = 0.0;
    lastFrameSeconds_ = 0.0;
    averageFrameSeconds_ = 0.0;
    frameIndex_ = 0;
}

void FrameTimer::BeginFrame()
{
    uint64_t now = NowTicks();
    uint64_t deltaTicks = now - lastTick_;
    lastTick_ = now;
    lastFrameSeconds_ = TicksToSeconds(deltaTicks);
    elapsedSeconds_ = TicksToSeconds(now - baseTick_);
    if (frameIndex_ == 0)
    {
        averageFrameSeconds_ = lastFrameSeconds_;
    }
    else
    {
        averageFrameSeconds_ = averageFrameSeconds_ * 0.95 + lastFrameSeconds_ * 0.05;
    }
    ++frameIndex_;
}

} // namespace kizuri::core