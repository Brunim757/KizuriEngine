#pragma once

#include <cstdint>

namespace kizuri::core {

struct ProfileEntry
{
    const char* Name;
    uint64_t StartCycle;
    uint64_t EndCycle;
    uint32_t ThreadId;
    uint32_t Depth;
};

struct ProfileFrame
{
    uint32_t FrameIndex;
    uint32_t FirstEntry;
    uint32_t EntryCount;
};

class Profiler {
public:
    static bool Initialize(uint32_t entryCapacity = 1u << 16);
    static void Shutdown();
    static bool IsInitialized();

    static void BeginFrame();
    static void EndFrame();

    static void PushScope(const char* name);
    static void PopScope();

    static uint32_t FrameCount();
    static ProfileFrame FrameAt(uint32_t frameIndexFromNewest);
    static ProfileEntry EntryAt(uint32_t absoluteIndex);
};

class ProfilerScope {
public:
    explicit ProfilerScope(const char* name);
    ProfilerScope(const ProfilerScope&) = delete;
    ProfilerScope& operator=(const ProfilerScope&) = delete;
    ~ProfilerScope();

private:
    const char* name_;
    uint64_t startCycle_;
};

} // namespace kizuri::core

#define KZ_PROFILE_CONCAT(a, b) KZ_PROFILE_CONCAT_IMPL(a, b)
#define KZ_PROFILE_CONCAT_IMPL(a, b) a##b
#define KZ_PROFILE_SCOPE(name) ::kizuri::core::ProfilerScope KZ_PROFILE_CONCAT(kz_profile_scope_, __LINE__)(name)