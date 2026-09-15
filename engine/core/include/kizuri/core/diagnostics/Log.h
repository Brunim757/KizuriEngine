#pragma once

#include <cstdint>

namespace kizuri::core {

struct LogConfig
{
    const char* LogDirectory = "logs";
    const char* LogFileName = "kizuri.log";
    bool EnableConsole = true;
    bool EnableFile = true;
    uint32_t MaxFileSizeBytes = 2 * 1024 * 1024;
    uint32_t MaxRotatingFiles = 3;
};

class Log {
public:
    static bool Initialize(const LogConfig& config);
    static void Shutdown();
    static bool IsInitialized();

    static void Info(const char* message);
    static void Warn(const char* message);
    static void Error(const char* message);
    static void Debug(const char* message);

    static void InfoFormatted(const char* fmt, ...);
    static void WarnFormatted(const char* fmt, ...);
    static void ErrorFormatted(const char* fmt, ...);
};

} // namespace kizuri::core