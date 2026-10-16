#include "mt2mp/core/logger.h"
#include <chrono>
#include <filesystem>

namespace mt2mp {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

const char* Logger::category_name(LogCategory cat) {
    switch (cat) {
        case LogCategory::General:  return "GEN";
        case LogCategory::Hooks:    return "HOOK";
        case LogCategory::Network:  return "NET";
        case LogCategory::Steam:    return "STEAM";
        case LogCategory::Overlay:  return "UI";
        case LogCategory::Sync:     return "SYNC";
        case LogCategory::PhysFS:   return "FS";
        case LogCategory::OpenGL:   return "GL";
        case LogCategory::API:      return "API";
        default: return "???";
    }
}

const char* Logger::category_filename(LogCategory cat) {
    switch (cat) {
        case LogCategory::General:  return "mt2mp_general.log";
        case LogCategory::Hooks:    return "mt2mp_hooks.log";
        case LogCategory::Network:  return "mt2mp_network.log";
        case LogCategory::Steam:    return "mt2mp_steam.log";
        case LogCategory::Overlay:  return "mt2mp_overlay.log";
        case LogCategory::Sync:     return "mt2mp_sync.log";
        case LogCategory::PhysFS:   return "mt2mp_physfs.log";
        case LogCategory::OpenGL:   return "mt2mp_opengl.log";
        case LogCategory::API:      return "mt2mp_api.log";
        default: return "mt2mp_other.log";
    }
}

Logger::LogFile& Logger::get_file(LogCategory cat) {
    int key = static_cast<int>(cat);
    auto it = files_.find(key);
    if (it == files_.end()) {
        auto& lf = files_[key];
        std::string path = base_dir_ + "\\" + category_filename(cat);
        lf.file.open(path, std::ios::out | std::ios::trunc);
        return lf;
    }
    return it->second;
}

void Logger::init(const std::string& base_dir) {
    if (initialized_) return;
    base_dir_ = base_dir;

    // Create logs directory
    std::string log_dir = base_dir + "\\mt2mp_logs";
    std::filesystem::create_directories(log_dir);
    base_dir_ = log_dir;

    // Open combined log
    main_file_.open(base_dir + "\\mt2mp.log", std::ios::out | std::ios::trunc);

    initialized_ = true;
    log(LogCategory::General, LogLevel::Info, __FILE__, __LINE__, "Logger initialized: " + log_dir);
}

void Logger::shutdown() {
    if (!initialized_) return;
    log(LogCategory::General, LogLevel::Info, __FILE__, __LINE__, "Logger shutting down");
    for (auto& [k, lf] : files_) {
        if (lf.file.is_open()) lf.file.close();
    }
    files_.clear();
    if (main_file_.is_open()) main_file_.close();
    initialized_ = false;
}

void Logger::set_level(LogLevel level) { min_level_ = level; }

void Logger::log(LogCategory cat, LogLevel level, const char* file, int line, const std::string& msg) {
    if (level < min_level_) return;

    static const char* level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    char time_buf[32];
    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t);
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_buf);

    const char* filename = file;
    if (auto* p = strrchr(file, '\\')) filename = p + 1;
    if (auto* p = strrchr(filename, '/')) filename = p + 1;

    auto formatted = std::format("[{}.{:03d}] [{}] [{}] [{}:{}] {}\n",
        time_buf, static_cast<int>(ms.count()),
        level_names[static_cast<int>(level)],
        category_name(cat),
        filename, line, msg);

    // Write to category-specific log
    if (initialized_) {
        auto& lf = get_file(cat);
        std::lock_guard lock(lf.mutex);
        if (lf.file.is_open()) {
            lf.file << formatted;
            lf.file.flush();
        }
    }

    // Write to combined log
    {
        std::lock_guard lock(main_mutex_);
        if (main_file_.is_open()) {
            main_file_ << formatted;
            main_file_.flush();
        }
    }
}

} // namespace mt2mp
