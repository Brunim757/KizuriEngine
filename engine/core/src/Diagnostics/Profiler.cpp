#include "kizuri/core/diagnostics/Profiler.h"

#ifndef _WIN32
#error "KizuriCore Profiler targets the Windows platform."
#endif

#include <windows.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace kizuri::core {

namespace {

constexpr uint32_t EntryCapacity = 1u << 16;
constexpr uint32_t FrameCapacity = 1u << 10;
constexpr uint32_t MaxScopeDepth = 64;

struct Entry
{
    ProfileEntry Data{};
    std::atomic<bool> Committed{ false };
};

std::mutex g_commitMutex;
std::atomic<uint32_t> g_nextEntry{ 0 };
std::atomic<uint32_t> g_frameRingCursor{ 0 };
std::atomic<uint32_t> g_frameCount{ 0 };
uint32_t g_frameIndex = 0;
uint32_t g_framesAllocated = 0;
Entry g_entries[EntryCapacity];
ProfileFrame g_frames[FrameCapacity];
thread_local uint32_t tls_depth = 0;
thread_local uint32_t tls_openSlots[MaxScopeDepth] = { 0 };
bool g_initialized = false;

uint64_t FrameCycleClock()
{
#ifdef _MSC_VER
    return __rdtsc();
#else
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

uint32_t CurrentThreadId()
{
    return static_cast<uint32_t>(GetCurrentThreadId());
}

void CommitEntry(const char* name, uint64_t startCycle, uint64_t endCycle, uint32_t depth)
{
    std::lock_guard<std::mutex> guard(g_commitMutex);
    uint32_t slot = g_nextEntry.fetch_add(1, std::memory_order_relaxed) % EntryCapacity;
    Entry& entry = g_entries[slot];
    entry.Committed.store(false, std::memory_order_relaxed);
    entry.Data.Name = name;
    entry.Data.StartCycle = startCycle;
    entry.Data.EndCycle = endCycle;
    entry.Data.ThreadId = CurrentThreadId();
    entry.Data.Depth = depth;
    entry.Committed.store(true, std::memory_order_release);
}

} // namespace

bool Profiler::Initialize(uint32_t entryCapacity)
{
    (void)entryCapacity;
    if (g_initialized)
    {
        return true;
    }
    g_nextEntry.store(0, std::memory_order_relaxed);
    g_frameRingCursor.store(0, std::memory_order_relaxed);
    g_frameCount.store(0, std::memory_order_relaxed);
    g_frameIndex = 0;
    g_framesAllocated = 0;
    for (uint32_t i = 0; i < EntryCapacity; ++i)
    {
        g_entries[i].Committed.store(false, std::memory_order_relaxed);
    }
    for (uint32_t i = 0; i < FrameCapacity; ++i)
    {
        g_frames[i].FrameIndex = 0;
        g_frames[i].FirstEntry = 0;
        g_frames[i].EntryCount = 0;
    }
    g_initialized = true;
    return true;
}

void Profiler::Shutdown()
{
    if (!g_initialized)
    {
        return;
    }
    std::lock_guard<std::mutex> guard(g_commitMutex);
    g_nextEntry.store(0, std::memory_order_relaxed);
    g_frameRingCursor.store(0, std::memory_order_relaxed);
    g_frameCount.store(0, std::memory_order_relaxed);
    g_frameIndex = 0;
    g_framesAllocated = 0;
    g_initialized = false;
}

bool Profiler::IsInitialized()
{
    return g_initialized;
}

void Profiler::BeginFrame()
{
    if (!g_initialized)
    {
        return;
    }
    g_framesAllocated = g_nextEntry.load(std::memory_order_relaxed);
    CommitEntry("FrameBegin", FrameCycleClock(), 0, 0);
    CommitEntry("FrameMark", FrameCycleClock(), FrameCycleClock(), 0);
}

void Profiler::EndFrame()
{
    if (!g_initialized)
    {
        return;
    }
    CommitEntry("FrameEnd", FrameCycleClock(), 0, 0);
    uint32_t endEntry = g_nextEntry.load(std::memory_order_relaxed);
    uint32_t slot = g_frameRingCursor.fetch_add(1, std::memory_order_relaxed) % FrameCapacity;
    ProfileFrame& frame = g_frames[slot];
    frame.FrameIndex = g_frameIndex;
    frame.FirstEntry = g_framesAllocated;
    frame.EntryCount = endEntry - g_framesAllocated;
    ++g_frameIndex;
    g_frameCount.fetch_add(1, std::memory_order_relaxed);
}

void Profiler::PushScope(const char* name)
{
    if (!g_initialized)
    {
        return;
    }
    uint64_t start = FrameCycleClock();
    CommitEntry(name, start, 0, tls_depth);
    tls_openSlots[tls_depth] = g_nextEntry.load(std::memory_order_relaxed) - 1;
    ++tls_depth;
    if (tls_depth >= MaxScopeDepth)
    {
        tls_depth = MaxScopeDepth - 1;
    }
}

void Profiler::PopScope()
{
    if (!g_initialized)
    {
        return;
    }
    if (tls_depth == 0)
    {
        return;
    }
    --tls_depth;
    uint32_t slotIndex = tls_openSlots[tls_depth];
    std::lock_guard<std::mutex> guard(g_commitMutex);
    uint32_t slot = slotIndex % EntryCapacity;
    if (!g_entries[slot].Committed.load(std::memory_order_acquire))
    {
        return;
    }
    if (g_entries[slot].Data.EndCycle == 0)
    {
        g_entries[slot].Data.EndCycle = FrameCycleClock();
    }
}

ProfilerScope::ProfilerScope(const char* name)
    : name_(name)
    , startCycle_(FrameCycleClock())
{
    if (tls_depth < MaxScopeDepth)
    {
        ++tls_depth;
    }
}

ProfilerScope::~ProfilerScope()
{
    if (!g_initialized)
    {
        if (tls_depth > 0)
        {
            --tls_depth;
        }
        return;
    }
    CommitEntry(name_, startCycle_, FrameCycleClock(), tls_depth);
    if (tls_depth > 0)
    {
        --tls_depth;
    }
}

uint32_t Profiler::FrameCount()
{
    return g_frameCount.load(std::memory_order_relaxed);
}

ProfileFrame Profiler::FrameAt(uint32_t frameIndexFromNewest)
{
    uint32_t total = g_frameCount.load(std::memory_order_relaxed);
    if (frameIndexFromNewest >= total)
    {
        return ProfileFrame{};
    }
    uint32_t absoluteIndex = total - 1 - frameIndexFromNewest;
    uint32_t slot = absoluteIndex % FrameCapacity;
    return g_frames[slot];
}

ProfileEntry Profiler::EntryAt(uint32_t absoluteIndex)
{
    uint32_t slot = absoluteIndex % EntryCapacity;
    Entry& entry = g_entries[slot];
    if (!entry.Committed.load(std::memory_order_acquire))
    {
        return ProfileEntry{};
    }
    return entry.Data;
}

} // namespace kizuri::core