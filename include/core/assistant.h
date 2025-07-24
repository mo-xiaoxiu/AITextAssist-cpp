#pragma once

#include "common/types.h"
#include "config/config_manager.h"
#include "llm/llm_client.h"
#include "database/conversation_db.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace AITextAssistant {

// Assistant state
enum class AssistantState {
    IDLE,
    PROCESSING,
    ERROR
};

// Assistant event types
enum class AssistantEvent {
    STATE_CHANGED,
    RESPONSE_GENERATED,
    ERROR_OCCURRED
};

// Event callback
using AssistantEventCallback = std::function<void(AssistantEvent, const std::string&)>;

// Assistant configuration
struct AssistantConfig {
    bool auto_save_conversations = true;
    double response_timeout = 30.0; // seconds
    int max_conversation_history = 20;
};

class TextAssistant {
public:
    explicit TextAssistant(const std::string& config_file = "");
    ~TextAssistant();
    
    // Initialization
    bool initialize();
    bool isInitialized() const { return initialized_; }
    
    // Configuration management
    bool loadConfig(const std::string& config_file);
    bool saveConfig(const std::string& config_file = "") const;
    void setAssistantConfig(const AssistantConfig& config) { assistant_config_ = config; }
    const AssistantConfig& getAssistantConfig() const { return assistant_config_; }
    
    // Conversation management
    std::string startNewConversation(const std::string& title = "");
    bool loadConversation(const ConversationId& conversation_id);
    bool saveCurrentConversation();
    std::vector<Conversation> getRecentConversations(int limit = 10);
    bool deleteConversation(const ConversationId& conversation_id);
    
    // Text-based interaction
    std::string processTextInput(const std::string& input);
    std::string getCurrentConversationId() const { return current_conversation_id_; }
    std::vector<Message> getCurrentConversationHistory() const;
    

    
    // State management
    AssistantState getState() const { return current_state_; }
    void setState(AssistantState state);
    
    // Event handling
    void setEventCallback(AssistantEventCallback callback) { event_callback_ = callback; }
    
    // Provider management
    bool setLLMProvider(const LLMConfig& config);
    
    // Utility methods
    void clearConversationHistory();
    std::string getSystemInfo() const;
    bool testConnections();
    
    // Prompt management
    void setSystemPrompt(const std::string& prompt);
    std::string getSystemPrompt() const;
    bool loadPromptTemplate(const std::string& template_name);
    
    // Statistics
    int getTotalConversations() const;
    int getTotalMessages() const;

private:
    // Core components
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<LLMClient> llm_client_;
    std::unique_ptr<ConversationDB> database_;
    
    // State
    std::atomic<bool> initialized_;
    std::atomic<AssistantState> current_state_;
    AssistantConfig assistant_config_;
    
    // Current conversation
    ConversationId current_conversation_id_;
    std::vector<Message> conversation_history_;
    mutable std::mutex conversation_mutex_;
    
    // Event handling
    AssistantEventCallback event_callback_;
    
    // Configuration
    std::string config_file_path_;
    
    // Private methods
    void initializeComponents();
    bool validateConfiguration();
    
    // Message processing
    std::string generateResponse(const std::string& user_input);
    std::string buildPromptWithContext(const std::string& user_input);
    void addMessageToHistory(const Message& message);
    void trimConversationHistory();
    
    // Event handling
    void fireEvent(AssistantEvent event, const std::string& data = "");
    

    
    // Error handling
    void handleError(const std::string& error_message);
    
    // Utility
    std::string generateConversationTitle(const std::string& first_message);
    bool updateConversationInDatabase();
};

} // namespace AITextAssistant
