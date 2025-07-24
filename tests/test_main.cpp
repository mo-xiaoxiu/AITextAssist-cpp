#include <gtest/gtest.h>
#include "utils/logger.h"

using namespace AITextAssistant;

// Test environment setup
class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Initialize logger for tests
        Logger& logger = Logger::getInstance();
        logger.setLogLevel(LogLevel::ERROR); // Reduce noise during tests
    }
    
    void TearDown() override {
        // Cleanup if needed
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Add global test environment
    ::testing::AddGlobalTestEnvironment(new TestEnvironment);
    
    return RUN_ALL_TESTS();
}
