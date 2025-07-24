#include <gtest/gtest.h>
#include "config/config_manager.h"
#include <filesystem>
#include <fstream>

using namespace AITextAssistant;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_manager = std::make_unique<ConfigManager>();
        test_config_file = "test_config.json";
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_config_file)) {
            std::filesystem::remove(test_config_file);
        }
    }
    
    void createTestConfigFile(const std::string& content) {
        std::ofstream file(test_config_file);
        file << content;
        file.close();
    }
    
    std::unique_ptr<ConfigManager> config_manager;
    std::string test_config_file;
};

TEST_F(ConfigManagerTest, DefaultConfigurationLoads) {
    config_manager->loadDefaultConfig();
    
    const auto& app_config = config_manager->getAppConfig();
    
    EXPECT_FALSE(app_config.llm.provider.empty());
    EXPECT_FALSE(app_config.llm.api_endpoint.empty());
    EXPECT_GT(app_config.llm.temperature, 0.0);
    EXPECT_GT(app_config.llm.max_tokens, 0);
    
    EXPECT_FALSE(app_config.prompt.system_prompt.empty());
    EXPECT_GT(app_config.prompt.max_history_messages, 0);
    
    EXPECT_GT(app_config.audio.sample_rate, 0);
    EXPECT_GT(app_config.audio.channels, 0);
}

TEST_F(ConfigManagerTest, LoadValidConfigFile) {
    std::string config_content = R"({
        "llm": {
            "provider": "openai",
            "api_endpoint": "https://api.openai.com/v1/chat/completions",
            "api_key": "test-key",
            "model_name": "gpt-3.5-turbo",
            "temperature": 0.7,
            "max_tokens": 1000,
            "headers": {
                "Content-Type": "application/json"
            }
        },
        "prompt": {
            "system_prompt": "Test system prompt",
            "user_prompt_template": "User: {user_input}",
            "context_template": "Context: {history}",
            "max_history_messages": 5
        },
        "audio": {
            "speech_to_text_provider": "whisper",
            "text_to_speech_provider": "espeak",
            "input_device": "default",
            "output_device": "default",
            "sample_rate": 16000,
            "channels": 1
        },
        "database_path": "test.db",
        "log_level": "DEBUG",
        "enable_voice": true,
        "auto_save_conversations": true
    })";
    
    createTestConfigFile(config_content);
    
    EXPECT_TRUE(config_manager->loadConfig(test_config_file));
    
    const auto& app_config = config_manager->getAppConfig();
    EXPECT_EQ(app_config.llm.provider, "openai");
    EXPECT_EQ(app_config.llm.api_key, "test-key");
    EXPECT_EQ(app_config.llm.temperature, 0.7);
    EXPECT_EQ(app_config.prompt.system_prompt, "Test system prompt");
    EXPECT_EQ(app_config.prompt.max_history_messages, 5);
    EXPECT_EQ(app_config.audio.sample_rate, 16000);
    EXPECT_EQ(app_config.database_path, "test.db");
    EXPECT_EQ(app_config.log_level, "DEBUG");
}

TEST_F(ConfigManagerTest, LoadInvalidConfigFile) {
    std::string invalid_config = R"({
        "llm": {
            "provider": "",
            "api_endpoint": "",
            "temperature": -1.0,
            "max_tokens": -100
        }
    })";
    
    createTestConfigFile(invalid_config);
    
    // Should load but validation should fail
    EXPECT_FALSE(config_manager->loadConfig(test_config_file));
}

TEST_F(ConfigManagerTest, SaveConfigFile) {
    config_manager->loadDefaultConfig();
    
    EXPECT_TRUE(config_manager->saveConfig(test_config_file));
    EXPECT_TRUE(std::filesystem::exists(test_config_file));
    
    // Try to load the saved config
    ConfigManager new_manager;
    EXPECT_TRUE(new_manager.loadConfig(test_config_file));
}

TEST_F(ConfigManagerTest, PromptTemplateManagement) {
    auto templates = config_manager->getAvailablePromptTemplates();
    EXPECT_FALSE(templates.empty());
    
    // Test loading different templates
    for (const auto& template_name : templates) {
        EXPECT_TRUE(config_manager->loadPromptTemplate(template_name));
    }
}

TEST_F(ConfigManagerTest, TemplateExpansion) {
    std::string template_str = "Hello {name}, your age is {age}";
    std::map<std::string, std::string> variables = {
        {"name", "John"},
        {"age", "25"}
    };
    
    std::string result = config_manager->expandTemplate(template_str, variables);
    EXPECT_EQ(result, "Hello John, your age is 25");
}

TEST_F(ConfigManagerTest, ConfigValidation) {
    config_manager->loadDefaultConfig();
    EXPECT_TRUE(config_manager->validateConfig());
    
    // Test invalid LLM config
    LLMConfig invalid_llm;
    invalid_llm.provider = "";  // Empty provider should be invalid
    config_manager->setLLMConfig(invalid_llm);
    // The setLLMConfig should reject invalid config, so validation should still pass
    EXPECT_TRUE(config_manager->validateConfig());
}

TEST_F(ConfigManagerTest, LLMConfigUpdate) {
    LLMConfig new_config;
    new_config.provider = "anthropic";
    new_config.api_endpoint = "https://api.anthropic.com/v1/messages";
    new_config.api_key = "test-anthropic-key";
    new_config.model_name = "claude-3-sonnet-20240229";
    new_config.temperature = 0.5;
    new_config.max_tokens = 2000;
    
    config_manager->setLLMConfig(new_config);
    
    const auto& updated_config = config_manager->getLLMConfig();
    EXPECT_EQ(updated_config.provider, "anthropic");
    EXPECT_EQ(updated_config.api_key, "test-anthropic-key");
    EXPECT_EQ(updated_config.temperature, 0.5);
}

TEST_F(ConfigManagerTest, PromptConfigUpdate) {
    PromptConfig new_config;
    new_config.system_prompt = "Custom system prompt";
    new_config.user_prompt_template = "Custom template: {user_input}";
    new_config.context_template = "Custom context: {history}";
    new_config.max_history_messages = 15;
    
    config_manager->setPromptConfig(new_config);
    
    const auto& updated_config = config_manager->getPromptConfig();
    EXPECT_EQ(updated_config.system_prompt, "Custom system prompt");
    EXPECT_EQ(updated_config.max_history_messages, 15);
}

TEST_F(ConfigManagerTest, AudioConfigUpdate) {
    AudioConfig new_config;
    new_config.speech_to_text_provider = "system";
    new_config.text_to_speech_provider = "system";
    new_config.input_device = "mic1";
    new_config.output_device = "speaker1";
    new_config.sample_rate = 44100;
    new_config.channels = 2;
    
    config_manager->setAudioConfig(new_config);
    
    const auto& updated_config = config_manager->getAudioConfig();
    EXPECT_EQ(updated_config.speech_to_text_provider, "system");
    EXPECT_EQ(updated_config.sample_rate, 44100);
    EXPECT_EQ(updated_config.channels, 2);
}
