# Test configuration
cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Download and build Google Test if not found
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50f33f9142fd2db563d69b413611e5.zip
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    test_config_manager.cpp
    test_llm_client.cpp
    test_conversation_db.cpp
    test_logger.cpp
)

# Create test executable
add_executable(run_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(run_tests
    PRIVATE
    gtest
    gtest_main
    Threads::Threads
    SQLite::SQLite3
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Add the same source files as the main project (except main.cpp)
target_sources(run_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/config/config_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/llm/llm_client.cpp
    ${CMAKE_SOURCE_DIR}/src/database/conversation_db.cpp
    ${CMAKE_SOURCE_DIR}/src/core/assistant.cpp
    ${CMAKE_SOURCE_DIR}/src/web/http_server.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logger.cpp
)

# Compiler flags
target_compile_options(run_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(run_tests)

# Custom test targets
add_custom_target(test_config
    COMMAND run_tests --gtest_filter="ConfigManager*"
    DEPENDS run_tests
    COMMENT "Running configuration tests"
)

add_custom_target(test_llm
    COMMAND run_tests --gtest_filter="LLMClient*"
    DEPENDS run_tests
    COMMENT "Running LLM client tests"
)

add_custom_target(test_db
    COMMAND run_tests --gtest_filter="ConversationDB*"
    DEPENDS run_tests
    COMMENT "Running database tests"
)



add_custom_target(test_logger
    COMMAND run_tests --gtest_filter="Logger*"
    DEPENDS run_tests
    COMMENT "Running logger tests"
)
