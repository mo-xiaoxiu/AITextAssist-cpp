#pragma once

#include "common/types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace AITextAssistant {

class ConfigManager {
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
    const AudioConfig& getAudioConfig() const { return app_config_.audio; }
    
    // Setters
    void setLLMConfig(const LLMConfig& config);
    void setPromptConfig(const PromptConfig& config);
    void setAudioConfig(const AudioConfig& config);
    
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
    bool validateAudioConfig(const AudioConfig& config) const;
    
    // Default configurations
    void setDefaultLLMConfig();
    void setDefaultPromptConfig();
    void setDefaultAudioConfig();
};

} // namespace AITextAssistant
