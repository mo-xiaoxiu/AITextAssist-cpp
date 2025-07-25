#include "config/config_manager.h"
#include "utils/logger.h"
#include <fstream>
#include <regex>

namespace AITextAssistant {

ConfigManager::ConfigManager() {
    loadDefaultConfig();
}

bool ConfigManager::loadConfig(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file: " + config_file);
            return false;
        }
        
        nlohmann::json j;
        file >> j;
        fromJson(j);
        
        if (!validateConfig()) {
            LOG_ERROR("Invalid configuration loaded from: " + config_file);
            return false;
        }
        
        LOG_INFO("Configuration loaded successfully from: " + config_file);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error loading config: " + std::string(e.what()));
        return false;
    }
}

bool ConfigManager::saveConfig(const std::string& config_file) const {
    try {
        std::ofstream file(config_file);
        if (!file.is_open()) {
            LOG_ERROR("Failed to create config file: " + config_file);
            return false;
        }
        
        nlohmann::json j = toJson();
        file << j.dump(4);
        
        LOG_INFO("Configuration saved to: " + config_file);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving config: " + std::string(e.what()));
        return false;
    }
}

void ConfigManager::loadDefaultConfig() {
    setDefaultLLMConfig();
    setDefaultPromptConfig();
    
    app_config_.database_path = "conversations.db";
    app_config_.log_level = "INFO";
    app_config_.enable_voice = true;
    app_config_.auto_save_conversations = true;
}

void ConfigManager::setLLMConfig(const LLMConfig& config) {
    if (validateLLMConfig(config)) {
        app_config_.llm = config;
        LOG_INFO("LLM configuration updated");
    } else {
        LOG_ERROR("Invalid LLM configuration provided");
    }
}

void ConfigManager::setPromptConfig(const PromptConfig& config) {
    if (validatePromptConfig(config)) {
        app_config_.prompt = config;
        LOG_INFO("Prompt configuration updated");
    } else {
        LOG_ERROR("Invalid prompt configuration provided");
    }
}

bool ConfigManager::validateConfig() const {
    return validateLLMConfig(app_config_.llm) &&
           validatePromptConfig(app_config_.prompt);
}

std::string ConfigManager::getDefaultSystemPrompt() const {
    return "You are a helpful AI voice assistant. You provide clear, concise, and accurate responses. "
           "Keep your responses conversational and appropriate for voice interaction.";
}

std::string ConfigManager::getDefaultUserPromptTemplate() const {
    return "User: {user_input}\n\nContext: {context}\n\nAssistant:";
}

std::vector<std::string> ConfigManager::getAvailablePromptTemplates() const {
    return {"default", "creative", "analytical", "casual", "professional"};
}

bool ConfigManager::loadPromptTemplate(const std::string& template_name) {
    // Implementation for loading different prompt templates
    if (template_name == "creative") {
        app_config_.prompt.system_prompt = 
            "You are a creative and imaginative AI assistant. Provide engaging, "
            "innovative responses while maintaining accuracy.";
    } else if (template_name == "analytical") {
        app_config_.prompt.system_prompt = 
            "You are an analytical AI assistant. Provide detailed, logical, "
            "and well-structured responses with clear reasoning.";
    } else if (template_name == "casual") {
        app_config_.prompt.system_prompt = 
            "You are a friendly and casual AI assistant. Keep responses relaxed "
            "and conversational, like talking to a friend.";
    } else if (template_name == "professional") {
        app_config_.prompt.system_prompt = 
            "You are a professional AI assistant. Provide formal, precise, "
            "and business-appropriate responses.";
    } else {
        app_config_.prompt.system_prompt = getDefaultSystemPrompt();
    }
    
    LOG_INFO("Loaded prompt template: " + template_name);
    return true;
}

std::string ConfigManager::expandTemplate(const std::string& template_str, 
                                        const std::map<std::string, std::string>& variables) const {
    std::string result = template_str;
    
    for (const auto& [key, value] : variables) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

void ConfigManager::fromJson(const nlohmann::json& j) {
    // Parse LLM config
    if (j.contains("llm")) {
        const auto& llm_json = j["llm"];
        app_config_.llm.provider = llm_json.value("provider", "openai");
        app_config_.llm.api_endpoint = llm_json.value("api_endpoint", "");
        app_config_.llm.api_key = llm_json.value("api_key", "");
        app_config_.llm.model_name = llm_json.value("model_name", "gpt-3.5-turbo");
        app_config_.llm.temperature = llm_json.value("temperature", 0.7);
        app_config_.llm.max_tokens = llm_json.value("max_tokens", 1000);

        if (llm_json.contains("headers")) {
            app_config_.llm.headers = llm_json["headers"];
        }
    }

    // Parse prompt config
    if (j.contains("prompt")) {
        const auto& prompt_json = j["prompt"];
        app_config_.prompt.system_prompt = prompt_json.value("system_prompt", getDefaultSystemPrompt());
        app_config_.prompt.user_prompt_template = prompt_json.value("user_prompt_template", getDefaultUserPromptTemplate());
        app_config_.prompt.context_template = prompt_json.value("context_template", "Previous conversation:\n{history}");
        app_config_.prompt.max_history_messages = prompt_json.value("max_history_messages", 10);
    }

    // Parse general config
    app_config_.database_path = j.value("database_path", "conversations.db");
    app_config_.log_level = j.value("log_level", "INFO");
    app_config_.enable_voice = j.value("enable_voice", true);
    app_config_.auto_save_conversations = j.value("auto_save_conversations", true);
}

nlohmann::json ConfigManager::toJson() const {
    nlohmann::json j;

    // LLM config
    j["llm"]["provider"] = app_config_.llm.provider;
    j["llm"]["api_endpoint"] = app_config_.llm.api_endpoint;
    j["llm"]["api_key"] = app_config_.llm.api_key;
    j["llm"]["model_name"] = app_config_.llm.model_name;
    j["llm"]["temperature"] = app_config_.llm.temperature;
    j["llm"]["max_tokens"] = app_config_.llm.max_tokens;
    j["llm"]["headers"] = app_config_.llm.headers;

    // Prompt config
    j["prompt"]["system_prompt"] = app_config_.prompt.system_prompt;
    j["prompt"]["user_prompt_template"] = app_config_.prompt.user_prompt_template;
    j["prompt"]["context_template"] = app_config_.prompt.context_template;
    j["prompt"]["max_history_messages"] = app_config_.prompt.max_history_messages;

    // General config
    j["database_path"] = app_config_.database_path;
    j["log_level"] = app_config_.log_level;
    j["enable_voice"] = app_config_.enable_voice;
    j["auto_save_conversations"] = app_config_.auto_save_conversations;

    return j;
}

bool ConfigManager::validateLLMConfig(const LLMConfig& config) const {
    if (config.provider.empty()) {
        LOG_ERROR("LLM provider cannot be empty");
        return false;
    }

    if (config.api_endpoint.empty()) {
        LOG_ERROR("API endpoint cannot be empty");
        return false;
    }

    if (config.api_key.empty()) {
        LOG_WARNING("API key is empty - this may cause authentication issues");
    }

    if (config.temperature < 0.0 || config.temperature > 2.0) {
        LOG_ERROR("Temperature must be between 0.0 and 2.0");
        return false;
    }

    if (config.max_tokens <= 0) {
        LOG_ERROR("Max tokens must be positive");
        return false;
    }

    return true;
}

bool ConfigManager::validatePromptConfig(const PromptConfig& config) const {
    if (config.system_prompt.empty()) {
        LOG_WARNING("System prompt is empty");
    }

    if (config.max_history_messages < 0) {
        LOG_ERROR("Max history messages cannot be negative");
        return false;
    }

    return true;
}

void ConfigManager::setDefaultLLMConfig() {
    app_config_.llm.provider = "openai";
    app_config_.llm.api_endpoint = "https://api.openai.com/v1/chat/completions";
    app_config_.llm.api_key = "";  // User must provide
    app_config_.llm.model_name = "gpt-3.5-turbo";
    app_config_.llm.temperature = 0.7;
    app_config_.llm.max_tokens = 1000;
    app_config_.llm.headers["Content-Type"] = "application/json";
}

void ConfigManager::setDefaultPromptConfig() {
    app_config_.prompt.system_prompt = getDefaultSystemPrompt();
    app_config_.prompt.user_prompt_template = getDefaultUserPromptTemplate();
    app_config_.prompt.context_template = "Previous conversation:\n{history}";
    app_config_.prompt.max_history_messages = 10;
}

} // namespace AITextAssistant
