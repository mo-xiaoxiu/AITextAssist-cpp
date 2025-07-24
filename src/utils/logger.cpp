#include "utils/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <chrono>
#include <filesystem>

namespace AITextAssistant {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_level_ = level;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create directory if it doesn't exist
    std::filesystem::path file_path(filename);
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }

    log_file_ = std::make_unique<std::ofstream>(filename, std::ios::app);
    if (!log_file_->is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        log_file_.reset();
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = getCurrentTime();
    std::string level_str = levelToString(level);
    std::string log_message = "[" + timestamp + "] [" + level_str + "] " + message;
    
    // Output to console with color coding
    if (level >= LogLevel::ERROR) {
        std::cerr << "\033[31m" << log_message << "\033[0m" << std::endl;  // Red for errors
    } else if (level == LogLevel::WARNING) {
        std::cout << "\033[33m" << log_message << "\033[0m" << std::endl;  // Yellow for warnings
    } else if (level == LogLevel::INFO) {
        std::cout << "\033[32m" << log_message << "\033[0m" << std::endl;  // Green for info
    } else {
        std::cout << log_message << std::endl;  // Default for debug
    }
    
    // Output to file if configured
    if (log_file_ && log_file_->is_open()) {
        *log_file_ << log_message << std::endl;
        log_file_->flush();
    }
}

std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO ";
        case LogLevel::WARNING: return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNW";
    }
}

LogLevel Logger::stringToLogLevel(const std::string& level_str) {
    if (level_str == "DEBUG") return LogLevel::DEBUG;
    if (level_str == "INFO") return LogLevel::INFO;
    if (level_str == "WARNING" || level_str == "WARN") return LogLevel::WARNING;
    if (level_str == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO;  // Default
}

} // namespace AITextAssistant
