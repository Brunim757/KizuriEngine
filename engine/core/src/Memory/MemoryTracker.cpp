#include "kizuri/core/memory/MemoryTracker.h"

#include <cstdlib>

namespace kizuri::core {

namespace {

struct RawHeader
{
    size_t Size;
};

constexpr size_t RawHeaderBytes = sizeof(RawHeader);

} // namespace

MemoryTracker& MemoryTracker::Instance()
{
    static MemoryTracker instance;
    return instance;
}

void* MemoryTracker::AllocateRaw(size_t size)
{
    void* base = std::malloc(RawHeaderBytes + size);
    if (base == nullptr)
    {
        return nullptr;
    }

    RawHeader* header = static_cast<RawHeader*>(base);
    header->Size = size;
    TrackAlloc(size);
    return reinterpret_cast<unsigned char*>(base) + RawHeaderBytes;
}

void MemoryTracker::FreeRaw(void* pointer) noexcept
{
    if (pointer == nullptr)
    {
        return;
    }

    RawHeader* header = reinterpret_cast<RawHeader*>(reinterpret_cast<unsigned char*>(pointer) - RawHeaderBytes);
    TrackFree(header->Size);
    std::free(header);
}

void MemoryTracker::TrackAlloc(size_t bytes) noexcept
{
    size_t currentTotal = totalBytes_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    size_t currentPeak = peakBytes_.load(std::memory_order_relaxed);
    while (currentPeak < currentTotal)
    {
        if (peakBytes_.compare_exchange_weak(currentPeak, currentTotal, std::memory_order_relaxed))
        {
            break;
        }
    }
    allocationCount_.fetch_add(1, std::memory_order_relaxed);
}

void MemoryTracker::TrackFree(size_t bytes) noexcept
{
    totalBytes_.fetch_sub(bytes, std::memory_order_relaxed);
}

} // namespace kizuri::core