#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <mutex>

namespace AITextAssistant {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& getInstance();
    
    void setLogLevel(LogLevel level);
    void setLogFile(const std::string& filename);
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    template<typename... Args>
    void debug(const std::string& format, Args... args);
    
    template<typename... Args>
    void info(const std::string& format, Args... args);
    
    template<typename... Args>
    void warning(const std::string& format, Args... args);
    
    template<typename... Args>
    void error(const std::string& format, Args... args);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void log(LogLevel level, const std::string& message);
    std::string getCurrentTime();
    std::string levelToString(LogLevel level);

public:
    // Helper function to convert string to LogLevel
    static LogLevel stringToLogLevel(const std::string& level_str);
    
    LogLevel current_level_ = LogLevel::INFO;
    std::unique_ptr<std::ofstream> log_file_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)

} // namespace AITextAssistant
