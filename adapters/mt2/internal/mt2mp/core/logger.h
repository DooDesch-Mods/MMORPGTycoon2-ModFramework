#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <format>
#include <unordered_map>

namespace mt2mp {

enum class LogLevel { Debug, Info, Warn, Error };

enum class LogCategory {
    General,
    Hooks,
    Network,
    Steam,
    Overlay,
    Sync,
    PhysFS,
    OpenGL,
    API
};

class Logger {
public:
    static Logger& instance();

    void init(const std::string& base_dir);
    void shutdown();
    void set_level(LogLevel level);

    void log(LogCategory cat, LogLevel level, const char* file, int line, const std::string& msg);

    template<typename... Args>
    void debug(LogCategory cat, const char* file, int line, std::format_string<Args...> fmt, Args&&... args) {
        log(cat, LogLevel::Debug, file, line, std::format(fmt, std::forward<Args>(args)...));
    }
    template<typename... Args>
    void info(LogCategory cat, const char* file, int line, std::format_string<Args...> fmt, Args&&... args) {
        log(cat, LogLevel::Info, file, line, std::format(fmt, std::forward<Args>(args)...));
    }
    template<typename... Args>
    void warn(LogCategory cat, const char* file, int line, std::format_string<Args...> fmt, Args&&... args) {
        log(cat, LogLevel::Warn, file, line, std::format(fmt, std::forward<Args>(args)...));
    }
    template<typename... Args>
    void error(LogCategory cat, const char* file, int line, std::format_string<Args...> fmt, Args&&... args) {
        log(cat, LogLevel::Error, file, line, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    Logger() = default;

    struct LogFile {
        std::ofstream file;
        std::recursive_mutex mutex;
    };

    std::unordered_map<int, LogFile> files_;
    std::ofstream main_file_; // combined log
    std::recursive_mutex main_mutex_;
    LogLevel min_level_ = LogLevel::Debug;
    bool initialized_ = false;
    std::string base_dir_;

    LogFile& get_file(LogCategory cat);
    static const char* category_name(LogCategory cat);
    static const char* category_filename(LogCategory cat);
};

// Macros with category
#define LOG_DEBUG(...)       ::mt2mp::Logger::instance().debug(::mt2mp::LogCategory::General, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)        ::mt2mp::Logger::instance().info(::mt2mp::LogCategory::General, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)        ::mt2mp::Logger::instance().warn(::mt2mp::LogCategory::General, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)       ::mt2mp::Logger::instance().error(::mt2mp::LogCategory::General, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_HOOKS_DEBUG(...) ::mt2mp::Logger::instance().debug(::mt2mp::LogCategory::Hooks, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_HOOKS_INFO(...)  ::mt2mp::Logger::instance().info(::mt2mp::LogCategory::Hooks, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_NET_DEBUG(...)   ::mt2mp::Logger::instance().debug(::mt2mp::LogCategory::Network, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_NET_INFO(...)    ::mt2mp::Logger::instance().info(::mt2mp::LogCategory::Network, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_NET_WARN(...)    ::mt2mp::Logger::instance().warn(::mt2mp::LogCategory::Network, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_NET_ERROR(...)   ::mt2mp::Logger::instance().error(::mt2mp::LogCategory::Network, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_STEAM_DEBUG(...) ::mt2mp::Logger::instance().debug(::mt2mp::LogCategory::Steam, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_STEAM_INFO(...)  ::mt2mp::Logger::instance().info(::mt2mp::LogCategory::Steam, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_STEAM_WARN(...)  ::mt2mp::Logger::instance().warn(::mt2mp::LogCategory::Steam, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_STEAM_ERROR(...) ::mt2mp::Logger::instance().error(::mt2mp::LogCategory::Steam, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_SYNC_DEBUG(...)  ::mt2mp::Logger::instance().debug(::mt2mp::LogCategory::Sync, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_SYNC_INFO(...)   ::mt2mp::Logger::instance().info(::mt2mp::LogCategory::Sync, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_FS_DEBUG(...)    ::mt2mp::Logger::instance().debug(::mt2mp::LogCategory::PhysFS, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FS_INFO(...)     ::mt2mp::Logger::instance().info(::mt2mp::LogCategory::PhysFS, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_API_DEBUG(...)   ::mt2mp::Logger::instance().debug(::mt2mp::LogCategory::API, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_API_INFO(...)    ::mt2mp::Logger::instance().info(::mt2mp::LogCategory::API, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_API_WARN(...)    ::mt2mp::Logger::instance().warn(::mt2mp::LogCategory::API, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_API_ERROR(...)   ::mt2mp::Logger::instance().error(::mt2mp::LogCategory::API, __FILE__, __LINE__, __VA_ARGS__)

} // namespace mt2mp
