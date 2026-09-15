#pragma once

#include <cstddef>
#include <cstdint>

#ifdef _MSC_VER
#define KIZURI_CALL __stdcall
#else
#define KIZURI_CALL
#endif

#define KIZURI_FORCEINLINE __forceinline

namespace kizuri {

inline constexpr size_t KernelAlignment = 16;
inline constexpr size_t PlatformCacheLineBytes = 64;

struct FrameInfo {
    uint32 Index;
    double SecondsElapsed;
    double FrameTimeSeconds;
};

} // namespace kizuri