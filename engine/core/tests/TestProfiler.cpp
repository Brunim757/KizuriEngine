#include <catch2/catch_test_macros.hpp>

#include "kizuri/core/diagnostics/Profiler.h"

namespace {

void ScopedWork()
{
    KZ_PROFILE_SCOPE("ScopedWork");
    volatile uint64_t sink = 0;
    for (uint32_t i = 0; i < 1000; ++i)
    {
        sink += i;
    }
    (void)sink;
}

} // namespace

TEST_CASE("Profiler captures a queryable frame", "[profiler]")
{
    REQUIRE(kizuri::core::Profiler::Initialize());
    kizuri::core::Profiler::BeginFrame();
    {
        KZ_PROFILE_SCOPE("FrameRoot");
        ScopedWork();
        KZ_PROFILE_SCOPE("FrameTail");
    }
    kizuri::core::Profiler::EndFrame();

    REQUIRE(kizuri::core::Profiler::FrameCount() >= 1);
    kizuri::core::ProfileFrame frame = kizuri::core::Profiler::FrameAt(0);
    REQUIRE(frame.EntryCount >= 3);

    uint32_t frameBoundaryEntries = 0;
    uint32_t scopeEntries = 0;
    for (uint32_t i = 0; i < frame.EntryCount; ++i)
    {
        kizuri::core::ProfileEntry entry = kizuri::core::Profiler::EntryAt(frame.FirstEntry + i);
        if (entry.Name == nullptr)
        {
            continue;
        }
        if (entry.Name[0] == 'F')
        {
            ++frameBoundaryEntries;
        }
        if (entry.Name[0] == 'S' || entry.Name[0] == 'W')
        {
            ++scopeEntries;
            REQUIRE(entry.EndCycle >= entry.StartCycle);
        }
    }

    REQUIRE(frameBoundaryEntries >= 3);
    REQUIRE(scopeEntries >= 1);

    kizuri::core::Profiler::Shutdown();
}