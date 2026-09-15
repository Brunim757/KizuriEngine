#include <catch2/catch_test_macros.hpp>

#include "kizuri/core/memory/LinearAllocator.h"
#include "kizuri/core/memory/MemoryTracker.h"
#include "kizuri/core/memory/StackAllocator.h"

TEST_CASE("MemoryTracker balances allocations and frees", "[memory][tracker]")
{
    kizuri::core::MemoryTracker& tracker = kizuri::core::MemoryTracker::Instance();
    const size_t baseline = tracker.TotalBytes();

    void* raw = tracker.AllocateRaw(100);
    REQUIRE(raw != nullptr);
    REQUIRE(tracker.TotalBytes() == baseline + 100);
    REQUIRE(tracker.PeakBytes() >= baseline + 100);

    tracker.FreeRaw(raw);
    REQUIRE(tracker.TotalBytes() == baseline);
}

TEST_CASE("MemoryTracker returns to baseline after allocator teardown", "[memory][tracker]")
{
    kizuri::core::MemoryTracker& tracker = kizuri::core::MemoryTracker::Instance();
    const size_t baseline = tracker.TotalBytes();

    {
        kizuri::core::LinearAllocator linear(2048);
        void* a = linear.Allocate(512);
        REQUIRE(a != nullptr);
        REQUIRE(tracker.TotalBytes() >= baseline + 2048);
    }
    REQUIRE(tracker.TotalBytes() == baseline);

    {
        kizuri::core::StackAllocator stack(2048);
        void* b = stack.Allocate(512);
        REQUIRE(b != nullptr);
    }
    REQUIRE(tracker.TotalBytes() == baseline);

    REQUIRE(tracker.TotalBytes() == baseline);
    REQUIRE(tracker.PeakBytes() >= baseline + 2048);
}