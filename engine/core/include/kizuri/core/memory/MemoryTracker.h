#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace kizuri::core {

class MemoryTracker {
public:
    static MemoryTracker& Instance();

    void* AllocateRaw(size_t size);
    void FreeRaw(void* pointer) noexcept;

    void TrackAlloc(size_t bytes) noexcept;
    void TrackFree(size_t bytes) noexcept;

    size_t TotalBytes() const noexcept { return totalBytes_.load(); }
    size_t PeakBytes() const noexcept { return peakBytes_.load(); }
    uint64_t AllocationCount() const noexcept { return allocationCount_.load(); }
    bool IsZeroed() const noexcept { return totalBytes_.load() == 0; }

    void ResetPeak() noexcept { peakBytes_.store(0); }

private:
    MemoryTracker() = default;

    std::atomic<size_t> totalBytes_{ 0 };
    std::atomic<size_t> peakBytes_{ 0 };
    std::atomic<uint64_t> allocationCount_{ 0 };
};

} // namespace kizuri::core