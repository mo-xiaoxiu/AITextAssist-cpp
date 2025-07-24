#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

namespace AITextAssistant {

// Forward declarations
class ConfigManager;
class LLMClient;
class ConversationDB;
class SpeechProcessor;

// Common types
using Timestamp = std::chrono::system_clock::time_point;
using MessageId = std::string;
using ConversationId = std::string;

// Message structure
struct Message {
    MessageId id;
    ConversationId conversation_id;
    std::string role;  // "user", "assistant", "system"
    std::string content;
    Timestamp timestamp;
    
    Message() = default;
    Message(const std::string& role, const std::string& content)
        : role(role), content(content), timestamp(std::chrono::system_clock::now()) {}
};

// Conversation structure
struct Conversation {
    ConversationId id;
    std::string title;
    std::vector<Message> messages;
    Timestamp created_at;
    Timestamp updated_at;
};

// LLM Configuration
struct LLMConfig {
    std::string provider;      // "openai", "anthropic", "custom"
    std::string api_endpoint;
    std::string api_key;
    std::string model_name;
    double temperature = 0.7;
    int max_tokens = 1000;
    std::map<std::string, std::string> headers;
};

// Prompt Configuration
struct PromptConfig {
    std::string system_prompt;
    std::string user_prompt_template;
    std::string context_template;
    int max_history_messages = 10;
};

// Audio Configuration
struct AudioConfig {
    std::string speech_to_text_provider;
    std::string text_to_speech_provider;
    std::string input_device;
    std::string output_device;
    int sample_rate = 16000;
    int channels = 1;
};

// Application Configuration
struct AppConfig {
    LLMConfig llm;
    PromptConfig prompt;
    AudioConfig audio;
    std::string database_path;
    std::string log_level = "INFO";
    bool enable_voice = true;
    bool auto_save_conversations = true;
};

// Response structure
struct LLMResponse {
    bool success;
    std::string content;
    std::string error_message;
    int status_code = 0;
    std::map<std::string, std::string> metadata;
};

// Audio data structure
struct AudioData {
    std::vector<uint8_t> data;
    int sample_rate;
    int channels;
    double duration;
};

} // namespace AITextAssistant
