cmake_minimum_required(VERSION 3.16)
project(AITextAssistant VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Find cURL for HTTP requests
find_package(CURL REQUIRED)

# Find nlohmann/json for JSON parsing
find_package(nlohmann_json REQUIRED)

# Find Threads for HTTP server
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Source files
set(SOURCES
    src/main.cpp
    src/config/config_manager.cpp
    src/llm/llm_client.cpp
    src/database/conversation_db.cpp
    src/core/assistant.cpp
    src/utils/logger.cpp
    src/web/http_server.cpp
)

# Header files
set(HEADERS
    include/config/config_manager.h
    include/llm/llm_client.h
    include/database/conversation_db.h
    include/core/assistant.h
    include/utils/logger.h
    include/common/types.h
    include/web/http_server.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Threads::Threads
    SQLite::SQLite3
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# Install configuration
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES config/default_config.json
    DESTINATION share/${PROJECT_NAME}/config
)

install(DIRECTORY web/
    DESTINATION share/${PROJECT_NAME}/web
    FILES_MATCHING PATTERN "*.html" PATTERN "*.css" PATTERN "*.js"
)

# Copy web files to build directory for development
file(COPY ${CMAKE_SOURCE_DIR}/web/
     DESTINATION ${CMAKE_BINARY_DIR}/web/
     FILES_MATCHING PATTERN "*.html" PATTERN "*.css" PATTERN "*.js"
)

# Testing
enable_testing()
add_subdirectory(tests)
