#pragma once

#include "common/types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace AITextAssistant {

class ConfigManager {
private:
    struct PromptTemplate {
        std::string name;
        std::string system_prompt;
        std::string description;
    };

    std::map<std::string, PromptTemplate> prompt_templates_ = {
        {"default", {"default", "You are a helpful AI assistant.", "A default prompt template."}},
        {"creative", {"creative", "You are a creative AI assistant.", "A creative prompt template."}},
        {"analytical", {"analytical", "You are an analytical AI assistant.", "An analytical prompt template."}},
        {"casual", {"casual", "You are a casual AI assistant.", "A casual prompt template."}},
        {"professional", {"professional", "You are a professional AI assistant.", "A professional prompt template."}}
    };

public:
    ConfigManager();
    ~ConfigManager() = default;
    
    // Load configuration from file
    bool loadConfig(const std::string& config_file);
    
    // Save configuration to file
    bool saveConfig(const std::string& config_file) const;
    
    // Load default configuration
    void loadDefaultConfig();
    
    // Getters
    const AppConfig& getAppConfig() const { return app_config_; }
    const LLMConfig& getLLMConfig() const { return app_config_.llm; }
    const PromptConfig& getPromptConfig() const { return app_config_.prompt; }
    
    // Setters
    void setLLMConfig(const LLMConfig& config);
    void setPromptConfig(const PromptConfig& config);
    
    // Validation
    bool validateConfig() const;
    
    // Template management
    std::string getDefaultSystemPrompt() const;
    std::string getDefaultUserPromptTemplate() const;
    std::vector<std::string> getAvailablePromptTemplates() const;
    bool loadPromptTemplate(const std::string& template_name);
    
    // Configuration helpers
    std::string expandTemplate(const std::string& template_str, 
                              const std::map<std::string, std::string>& variables) const;

private:
    AppConfig app_config_;
    
    // JSON conversion helpers
    void fromJson(const nlohmann::json& j);
    nlohmann::json toJson() const;
    
    // Validation helpers
    bool validateLLMConfig(const LLMConfig& config) const;
    bool validatePromptConfig(const PromptConfig& config) const;
    
    // Default configurations
    void setDefaultLLMConfig();
    void setDefaultPromptConfig();

    static constexpr double MIN_TEMPERATURE = 0.0;
    static constexpr double MAX_TEMPERATURE = 2.0;
    static constexpr double DEFAULT_TEMPERATURE = 0.7;
    static constexpr int DEFAULT_MAX_TOKENS = 1000;
    static constexpr int DEFAULT_MAX_HISTORY_LENGTH = 10;
    static constexpr std::string_view DEFAULT_PROVIDER = "openai";
    static constexpr std::string_view DEFAULT_MODEL_NAME = "gpt-4.1";
    static constexpr std::string_view DEFAULT_API_ENDPOINT = "https://api.openai.com/v1";
    static constexpr std::string_view DEFAULT_API_KEY = "";
    static constexpr std::string_view DEFAULT_DATABASE_PATH = "conversations.db";
};

} // namespace AITextAssistant
