#include <windows.h>

#include <atomic>
#include <cstdint>

#include "kizuri/core/diagnostics/Log.h"
#include "kizuri/core/diagnostics/Profiler.h"
#include "kizuri/core/jobs/JobSystem.h"
#include "kizuri/core/memory/MemoryTracker.h"
#include "kizuri/core/platform/Timer.h"
#include "kizuri/core/platform/Window.h"

namespace {

using namespace kizuri;
using namespace kizuri::core;

constexpr uint32_t FrameJobCount = 512;
constexpr uint32_t FrameCapDurationMs = 16;

std::atomic<uint64_t> g_frameSum{ 0 };

void SumFrameWork(void* userData, uint32_t index)
{
    std::atomic<uint64_t>* accumulator = static_cast<std::atomic<uint64_t>*>(userData);
    accumulator->fetch_add(static_cast<uint64_t>(index));
}

void EmitProfilerSummary()
{
    const uint32_t frameCount = Profiler::FrameCount();
    if (frameCount == 0)
    {
        Log::Info("Profiler captured no frames");
        return;
    }

    ProfileFrame latest = Profiler::FrameAt(0);
    uint32_t frameEntries = 0;
    uint32_t scopeEntries = 0;
    for (uint32_t i = 0; i < latest.EntryCount; ++i)
    {
        ++frameEntries;
        ProfileEntry entry = Profiler::EntryAt(latest.FirstEntry + i);
        if (entry.Name == nullptr)
        {
            continue;
        }
        if (entry.EndCycle >= entry.StartCycle && entry.EndCycle > 0)
        {
            ++scopeEntries;
        }
    }

    Log::InfoFormatted(
        "Profiler: %u frames captured, latest frame has %u entries, %u scopes measured",
        frameCount, frameEntries, scopeEntries);
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    LogConfig logConfig;
    logConfig.LogDirectory = "logs";
    logConfig.LogFileName = "kizuri.log";
    Log::Initialize(logConfig);
    Log::InfoFormatted("KizuriEngine Fase 0 startup");

    Profiler::Initialize();
    JobSystem::Initialize(4);
    Log::InfoFormatted("JobSystem initialized with %u workers", JobSystem::WorkerCount());

    FrameTimer timer;
    timer.Reset();

    Window window;
    WindowDesc desc;
    desc.Title = "Kizuri Engine - Fase 0";
    desc.Width = 1280;
    desc.Height = 720;
    if (!window.Create(desc))
    {
        Log::Error("Failed to create platform window");
        JobSystem::Shutdown();
        Profiler::Shutdown();
        Log::Shutdown();
        return 1;
    }

    while (!window.ShouldClose())
    {
        Profiler::BeginFrame();
        {
            KZ_PROFILE_SCOPE("FrameTotal");

            window.ProcessMessages();
            const Input& input = window.GetInput();

            if (input.WasPressed(Key::Escape))
            {
                Log::Info("Escape pressed, requesting window close");
                window.RequestClose();
            }

            timer.BeginFrame();

            KZ_PROFILE_SCOPE("FrameWork");
            if (JobSystem::IsInitialized())
            {
                g_frameSum.store(0);
                JobSystem::ParallelFor(FrameJobCount, &SumFrameWork, &g_frameSum);
            }

            if (timer.FrameIndex() % 300 == 0)
            {
                Log::InfoFormatted(
                    "frame %u | dt %.2f ms | sum %llu | mouse (%d, %d)",
                    timer.FrameIndex(),
                    timer.LastFrameSeconds() * 1000.0,
                    static_cast<unsigned long long>(g_frameSum.load()),
                    input.MouseX(),
                    input.MouseY());
            }

            const uint32_t frameMs = static_cast<uint32_t>(timer.LastFrameSeconds() * 1000.0);
            if (frameMs < FrameCapDurationMs)
            {
                Sleep(FrameCapDurationMs - frameMs);
            }
        }
        Profiler::EndFrame();
    }

    window.Destroy();

    EmitProfilerSummary();
    JobSystem::Shutdown();
    Profiler::Shutdown();

    const bool memoryClean = MemoryTracker::Instance().IsZeroed();
    Log::InfoFormatted(
        "MemoryTracker %s at shutdown (peak %zu bytes)",
        memoryClean ? "clean" : "LEAK DETECTED",
        MemoryTracker::Instance().PeakBytes());
    Log::Shutdown();

    return memoryClean ? 0 : 1;
}