#include "kizuri/core/diagnostics/Log.h"

#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace kizuri::core {

namespace {

std::shared_ptr<spdlog::logger> g_logger;

void FormatInto(std::string& buffer, const char* fmt, va_list args)
{
    buffer.resize(1024);
    int count = vsnprintf(buffer.data(), buffer.size(), fmt, args);
    if (count > static_cast<int>(buffer.size()))
    {
        buffer.resize(static_cast<size_t>(count) + 1);
        va_list replay;
        va_copy(replay, args);
        vsnprintf(buffer.data(), buffer.size(), fmt, replay);
        va_end(replay);
    }
    buffer.resize(static_cast<size_t>(count));
}

} // namespace

bool Log::Initialize(const LogConfig& config)
{
    if (g_logger != nullptr)
    {
        return true;
    }

    std::vector<spdlog::sink_ptr> sinks;
    if (config.EnableConsole)
    {
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    if (config.EnableFile)
    {
        std::error_code error;
        std::filesystem::create_directories(config.LogDirectory, error);
        std::string filePath = std::string(config.LogDirectory) + "/" + config.LogFileName;
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            filePath, config.MaxFileSizeBytes, config.MaxRotatingFiles, false));
    }

    if (sinks.empty())
    {
        return false;
    }

    g_logger = std::make_shared<spdlog::logger>("kizuri", sinks.begin(), sinks.end());
    g_logger->set_level(spdlog::level::trace);
    g_logger->set_pattern("[%H:%M:%S.%e] [%n] [%^%l%$] %v");
    g_logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(g_logger);
    return true;
}

void Log::Shutdown()
{
    if (g_logger != nullptr)
    {
        g_logger->flush();
        spdlog::shutdown();
        g_logger.reset();
    }
}

bool Log::IsInitialized()
{
    return g_logger != nullptr;
}

void Log::Info(const char* message)
{
    if (g_logger != nullptr)
    {
        g_logger->info(message);
    }
}

void Log::Warn(const char* message)
{
    if (g_logger != nullptr)
    {
        g_logger->warn(message);
    }
}

void Log::Error(const char* message)
{
    if (g_logger != nullptr)
    {
        g_logger->error(message);
    }
}

void Log::Debug(const char* message)
{
    if (g_logger != nullptr)
    {
        g_logger->debug(message);
    }
}

void Log::InfoFormatted(const char* fmt, ...)
{
    if (g_logger != nullptr)
    {
        std::string buffer;
        va_list args;
        va_start(args, fmt);
        FormatInto(buffer, fmt, args);
        va_end(args);
        g_logger->info(buffer);
    }
}

void Log::WarnFormatted(const char* fmt, ...)
{
    if (g_logger != nullptr)
    {
        std::string buffer;
        va_list args;
        va_start(args, fmt);
        FormatInto(buffer, fmt, args);
        va_end(args);
        g_logger->warn(buffer);
    }
}

void Log::ErrorFormatted(const char* fmt, ...)
{
    if (g_logger != nullptr)
    {
        std::string buffer;
        va_list args;
        va_start(args, fmt);
        FormatInto(buffer, fmt, args);
        va_end(args);
        g_logger->error(buffer);
    }
}

} // namespace kizuri::core