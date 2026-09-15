#pragma once

#include <cstdint>

#include "kizuri/core/CoreTypes.h"

namespace kizuri::core {

class FrameTimer {
public:
    void Reset();
    void BeginFrame();

    double ElapsedSeconds() const noexcept { return elapsedSeconds_; }
    double LastFrameSeconds() const noexcept { return lastFrameSeconds_; }
    double AverageFrameSeconds() const noexcept { return averageFrameSeconds_; }
    uint32_t FrameIndex() const noexcept { return frameIndex_; }

private:
    static uint64_t NowTicks() noexcept;
    static double TicksToSeconds(uint64_t ticks) noexcept;

    uint64_t lastTick_ = 0;
    uint64_t baseTick_ = 0;
    double elapsedSeconds_ = 0.0;
    double lastFrameSeconds_ = 0.0;
    double averageFrameSeconds_ = 0.0;
    uint32_t frameIndex_ = 0;
};

} // namespace kizuri::core