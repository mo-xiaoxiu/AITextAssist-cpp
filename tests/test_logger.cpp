#include <gtest/gtest.h>
#include "utils/logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

using namespace AITextAssistant;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_file = "test_log.txt";
        
        // Clean up any existing test log file
        if (std::filesystem::exists(test_log_file)) {
            std::filesystem::remove(test_log_file);
        }
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_log_file)) {
            std::filesystem::remove(test_log_file);
        }
    }
    
    std::string readLogFile() {
        if (!std::filesystem::exists(test_log_file)) {
            return "";
        }
        
        std::ifstream file(test_log_file);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    std::string test_log_file;
};

TEST_F(LoggerTest, SingletonInstance) {
    Logger& logger1 = Logger::getInstance();
    Logger& logger2 = Logger::getInstance();
    
    // Should be the same instance
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    Logger& logger = Logger::getInstance();
    
    // Set log level to WARNING
    logger.setLogLevel(LogLevel::WARNING);
    logger.setLogFile(test_log_file);
    
    // Log messages at different levels
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warning("Warning message");
    logger.error("Error message");
    
    std::string log_content = readLogFile();
    
    // Only WARNING and ERROR should be logged
    EXPECT_EQ(log_content.find("Debug message"), std::string::npos);
    EXPECT_EQ(log_content.find("Info message"), std::string::npos);
    EXPECT_NE(log_content.find("Warning message"), std::string::npos);
    EXPECT_NE(log_content.find("Error message"), std::string::npos);
}

TEST_F(LoggerTest, LogToFile) {
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    logger.setLogFile(test_log_file);
    
    std::string test_message = "Test log message";
    logger.info(test_message);
    
    std::string log_content = readLogFile();
    EXPECT_NE(log_content.find(test_message), std::string::npos);
    EXPECT_NE(log_content.find("[INFO ]"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelStrings) {
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    logger.setLogFile(test_log_file);
    
    logger.debug("Debug test");
    logger.info("Info test");
    logger.warning("Warning test");
    logger.error("Error test");
    
    std::string log_content = readLogFile();
    
    EXPECT_NE(log_content.find("[DEBUG]"), std::string::npos);
    EXPECT_NE(log_content.find("[INFO ]"), std::string::npos);
    EXPECT_NE(log_content.find("[WARN ]"), std::string::npos);
    EXPECT_NE(log_content.find("[ERROR]"), std::string::npos);
}

TEST_F(LoggerTest, TimestampFormat) {
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    logger.setLogFile(test_log_file);
    
    logger.info("Timestamp test");
    
    std::string log_content = readLogFile();
    
    // Check for timestamp format: [YYYY-MM-DD HH:MM:SS.mmm]
    // This is a basic check - we look for the pattern
    EXPECT_TRUE(std::regex_search(log_content, 
        std::regex(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\])")));
}

TEST_F(LoggerTest, StringToLogLevel) {
    EXPECT_EQ(Logger::stringToLogLevel("DEBUG"), LogLevel::DEBUG);
    EXPECT_EQ(Logger::stringToLogLevel("INFO"), LogLevel::INFO);
    EXPECT_EQ(Logger::stringToLogLevel("WARNING"), LogLevel::WARNING);
    EXPECT_EQ(Logger::stringToLogLevel("WARN"), LogLevel::WARNING);
    EXPECT_EQ(Logger::stringToLogLevel("ERROR"), LogLevel::ERROR);
    EXPECT_EQ(Logger::stringToLogLevel("UNKNOWN"), LogLevel::INFO); // Default
}

TEST_F(LoggerTest, DirectoryCreation) {
    std::string nested_log_file = "test_dir/nested/log.txt";
    
    Logger& logger = Logger::getInstance();
    logger.setLogFile(nested_log_file);
    logger.info("Directory creation test");
    
    EXPECT_TRUE(std::filesystem::exists(nested_log_file));
    
    // Clean up
    std::filesystem::remove_all("test_dir");
}

TEST_F(LoggerTest, MultipleMessages) {
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    logger.setLogFile(test_log_file);
    
    std::vector<std::string> messages = {
        "First message",
        "Second message",
        "Third message"
    };
    
    for (const auto& msg : messages) {
        logger.info(msg);
    }
    
    std::string log_content = readLogFile();
    
    for (const auto& msg : messages) {
        EXPECT_NE(log_content.find(msg), std::string::npos);
    }
}

TEST_F(LoggerTest, LogLevelChanges) {
    Logger& logger = Logger::getInstance();
    logger.setLogFile(test_log_file);
    
    // Start with ERROR level
    logger.setLogLevel(LogLevel::ERROR);
    logger.info("This should not appear");
    logger.error("This should appear");
    
    // Change to DEBUG level
    logger.setLogLevel(LogLevel::DEBUG);
    logger.debug("This should now appear");
    
    std::string log_content = readLogFile();
    
    EXPECT_EQ(log_content.find("This should not appear"), std::string::npos);
    EXPECT_NE(log_content.find("This should appear"), std::string::npos);
    EXPECT_NE(log_content.find("This should now appear"), std::string::npos);
}
