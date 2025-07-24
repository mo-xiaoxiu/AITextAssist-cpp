#include "core/assistant.h"
#include "utils/logger.h"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <regex>
#include <stdexcept>

namespace AITextAssistant {

TextAssistant::TextAssistant(const std::string& config_file)
    : initialized_(false), current_state_(AssistantState::IDLE), config_file_path_(config_file) {

    // Initialize default assistant configuration
    assistant_config_.auto_save_conversations = true;
    assistant_config_.response_timeout = 30.0;
    assistant_config_.max_conversation_history = 20;
}

TextAssistant::~TextAssistant() {
    if (assistant_config_.auto_save_conversations && !current_conversation_id_.empty()) {
        saveCurrentConversation();
    }
}

bool TextAssistant::initialize() {
    try {
        LOG_INFO("Initializing Text Assistant...");

        // Initialize configuration manager
        config_manager_ = std::make_unique<ConfigManager>();

        if (!config_file_path_.empty()) {
            if (!config_manager_->loadConfig(config_file_path_)) {
                LOG_WARNING("Failed to load config file, using defaults");
            }
        } else {
            config_manager_->loadDefaultConfig();
        }

        if (!validateConfiguration()) {
            LOG_ERROR("Configuration validation failed");
            return false;
        }

        // Initialize components
        initializeComponents();

        initialized_ = true;
        setState(AssistantState::IDLE);

        LOG_INFO("Text Assistant initialized successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Exception during initialization: " + std::string(e.what()));
        return false;
    }
}

void TextAssistant::initializeComponents() {
    const auto& app_config = config_manager_->getAppConfig();

    // Initialize database
    database_ = std::make_unique<ConversationDB>(app_config.database_path);
    if (!database_->initialize()) {
        throw std::runtime_error("Failed to initialize database");
    }

    // Initialize LLM client
    llm_client_ = LLMClient::createClient(app_config.llm);
    if (!llm_client_) {
        throw std::runtime_error("Failed to create LLM client");
    }


}

bool TextAssistant::validateConfiguration() {
    return config_manager_->validateConfig();
}

bool TextAssistant::loadConfig(const std::string& config_file) {
    if (!config_manager_) {
        LOG_ERROR("Configuration manager not initialized");
        return false;
    }

    config_file_path_ = config_file;
    return config_manager_->loadConfig(config_file);
}

bool TextAssistant::saveConfig(const std::string& config_file) const {
    if (!config_manager_) {
        LOG_ERROR("Configuration manager not initialized");
        return false;
    }

    std::string file_path = config_file.empty() ? config_file_path_ : config_file;
    return config_manager_->saveConfig(file_path);
}

std::string TextAssistant::startNewConversation(const std::string& title) {
    if (!database_) {
        LOG_ERROR("Database not initialized");
        return "";
    }

    std::lock_guard<std::mutex> lock(conversation_mutex_);

    // Save current conversation if exists
    if (!current_conversation_id_.empty() && assistant_config_.auto_save_conversations) {
        updateConversationInDatabase();
    }

    // Create new conversation
    current_conversation_id_ = database_->createConversation(title);
    conversation_history_.clear();

    if (!current_conversation_id_.empty()) {
        LOG_INFO("Started new conversation: " + current_conversation_id_);
        fireEvent(AssistantEvent::STATE_CHANGED, "New conversation started");
    }

    return current_conversation_id_;
}

bool TextAssistant::loadConversation(const ConversationId& conversation_id) {
    if (!database_) {
        LOG_ERROR("Database not initialized");
        return false;
    }

    std::lock_guard<std::mutex> lock(conversation_mutex_);

    auto conversation = database_->getConversation(conversation_id);
    if (!conversation.has_value()) {
        LOG_ERROR("Conversation not found: " + conversation_id);
        return false;
    }

    // Save current conversation if exists
    if (!current_conversation_id_.empty() && assistant_config_.auto_save_conversations) {
        updateConversationInDatabase();
    }

    current_conversation_id_ = conversation_id;
    conversation_history_ = conversation->messages;

    LOG_INFO("Loaded conversation: " + conversation_id);
    return true;
}

bool TextAssistant::saveCurrentConversation() {
    if (current_conversation_id_.empty() || !database_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(conversation_mutex_);
    return updateConversationInDatabase();
}

std::vector<Conversation> TextAssistant::getRecentConversations(int limit) {
    if (!database_) {
        return {};
    }
    return database_->getRecentConversations(limit);
}

bool TextAssistant::deleteConversation(const ConversationId& conversation_id) {
    if (!database_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(conversation_mutex_);

    // If deleting current conversation, clear it
    if (current_conversation_id_ == conversation_id) {
        current_conversation_id_.clear();
        conversation_history_.clear();
    }

    return database_->deleteConversation(conversation_id);
}

std::string TextAssistant::processTextInput(const std::string& input) {
    if (!initialized_ || input.empty()) {
        return "Sorry, I'm not ready to process your request.";
    }

    setState(AssistantState::PROCESSING);

    try {
        // Add user message to history
        Message user_message("user", input);
        addMessageToHistory(user_message);

        // Generate response
        std::string response = generateResponse(input);

        // Add assistant response to history
        Message assistant_message("assistant", response);
        addMessageToHistory(assistant_message);

        setState(AssistantState::IDLE);
        fireEvent(AssistantEvent::RESPONSE_GENERATED, response);

        return response;

    } catch (const std::exception& e) {
        setState(AssistantState::ERROR);
        std::string error_msg = "Error processing input: " + std::string(e.what());
        LOG_ERROR(error_msg);
        fireEvent(AssistantEvent::ERROR_OCCURRED, error_msg);
        return "I'm sorry, I encountered an error processing your request.";
    }
}

std::vector<Message> TextAssistant::getCurrentConversationHistory() const {
    std::lock_guard<std::mutex> lock(conversation_mutex_);
    return conversation_history_;
}



void TextAssistant::setState(AssistantState state) {
    current_state_ = state;

    std::string state_str;
    switch (state) {
        case AssistantState::IDLE: state_str = "IDLE"; break;
        case AssistantState::PROCESSING: state_str = "PROCESSING"; break;
        case AssistantState::ERROR: state_str = "ERROR"; break;
    }

    fireEvent(AssistantEvent::STATE_CHANGED, state_str);
}

bool TextAssistant::setLLMProvider(const LLMConfig& config) {
    if (!config_manager_) {
        return false;
    }

    config_manager_->setLLMConfig(config);

    // Recreate LLM client with new config
    try {
        llm_client_ = LLMClient::createClient(config);
        LOG_INFO("LLM provider updated: " + config.provider);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update LLM provider: " + std::string(e.what()));
        return false;
    }
}



void TextAssistant::clearConversationHistory() {
    std::lock_guard<std::mutex> lock(conversation_mutex_);
    conversation_history_.clear();
    LOG_INFO("Conversation history cleared");
}

std::string TextAssistant::getSystemInfo() const {
    if (!config_manager_) {
        return "Configuration not available";
    }

    std::stringstream info;
    const auto& app_config = config_manager_->getAppConfig();

    info << "AI Text Assistant System Information:\n";
    info << "=====================================\n";
    info << "LLM Provider: " << app_config.llm.provider << "\n";
    info << "Model: " << app_config.llm.model_name << "\n";


    info << "Database: " << app_config.database_path << "\n";
    info << "Total Conversations: " << getTotalConversations() << "\n";
    info << "Total Messages: " << getTotalMessages() << "\n";

    return info.str();
}

bool TextAssistant::testConnections() {
    LOG_INFO("Testing connections...");

    bool all_ok = true;

    // Test database connection
    if (!database_ || !database_->isInitialized()) {
        LOG_ERROR("Database connection failed");
        all_ok = false;
    } else {
        LOG_INFO("Database connection: OK");
    }

    // Test LLM connection (basic validation)
    if (!llm_client_) {
        LOG_ERROR("LLM client not available");
        all_ok = false;
    } else {
        LOG_INFO("LLM client: OK");
    }



    return all_ok;
}

void TextAssistant::setSystemPrompt(const std::string& prompt) {
    if (!config_manager_) {
        return;
    }

    PromptConfig prompt_config = config_manager_->getPromptConfig();
    prompt_config.system_prompt = prompt;
    config_manager_->setPromptConfig(prompt_config);
}

std::string TextAssistant::getSystemPrompt() const {
    if (!config_manager_) {
        return "";
    }

    return config_manager_->getPromptConfig().system_prompt;
}

bool TextAssistant::loadPromptTemplate(const std::string& template_name) {
    if (!config_manager_) {
        return false;
    }

    return config_manager_->loadPromptTemplate(template_name);
}

int TextAssistant::getTotalConversations() const {
    if (!database_) {
        return 0;
    }
    return database_->getConversationCount();
}

int TextAssistant::getTotalMessages() const {
    if (!database_) {
        return 0;
    }
    return database_->getMessageCount();
}

std::string TextAssistant::generateResponse(const std::string& user_input) {
    if (!llm_client_) {
        return "I'm sorry, I'm not able to process your request right now.";
    }

    // Build prompt with context
    std::string prompt = buildPromptWithContext(user_input);

    // Prepare messages for LLM
    std::vector<Message> messages;

    // Add system message
    const auto& prompt_config = config_manager_->getPromptConfig();
    if (!prompt_config.system_prompt.empty()) {
        messages.emplace_back("system", prompt_config.system_prompt);
    }

    // Add conversation history (limited)
    std::lock_guard<std::mutex> lock(conversation_mutex_);
    int history_limit = std::min(assistant_config_.max_conversation_history,
                                static_cast<int>(conversation_history_.size()));

    if (history_limit > 0) {
        int start_idx = conversation_history_.size() - history_limit;
        for (int i = start_idx; i < static_cast<int>(conversation_history_.size()); ++i) {
            messages.push_back(conversation_history_[i]);
        }
    }

    // Add current user input
    messages.emplace_back("user", user_input);

    // Get response from LLM
    LLMResponse response = llm_client_->chatCompletion(messages);

    if (response.success) {
        return response.content;
    } else {
        LOG_ERROR("LLM request failed: " + response.error_message);
        return "I'm sorry, I encountered an error while processing your request.";
    }
}

std::string TextAssistant::buildPromptWithContext(const std::string& user_input) {
    if (!config_manager_) {
        return user_input;
    }

    const auto& prompt_config = config_manager_->getPromptConfig();

    // Build context from conversation history
    std::string context;
    if (!conversation_history_.empty()) {
        std::stringstream history_stream;

        int history_limit = std::min(prompt_config.max_history_messages,
                                   static_cast<int>(conversation_history_.size()));

        int start_idx = conversation_history_.size() - history_limit;
        for (int i = start_idx; i < static_cast<int>(conversation_history_.size()); ++i) {
            const auto& msg = conversation_history_[i];
            history_stream << msg.role << ": " << msg.content << "\n";
        }

        std::map<std::string, std::string> context_vars = {
            {"history", history_stream.str()}
        };
        context = config_manager_->expandTemplate(prompt_config.context_template, context_vars);
    }

    // Build final prompt
    std::map<std::string, std::string> prompt_vars = {
        {"user_input", user_input},
        {"context", context}
    };

    return config_manager_->expandTemplate(prompt_config.user_prompt_template, prompt_vars);
}

void TextAssistant::addMessageToHistory(const Message& message) {
    std::lock_guard<std::mutex> lock(conversation_mutex_);

    // Set conversation ID and timestamp
    Message msg = message;
    msg.conversation_id = current_conversation_id_;
    msg.timestamp = std::chrono::system_clock::now();

    conversation_history_.push_back(msg);

    // Add to database if auto-save is enabled
    if (assistant_config_.auto_save_conversations && database_ && !current_conversation_id_.empty()) {
        database_->addMessage(current_conversation_id_, msg);
    }

    // Trim history if it gets too long
    trimConversationHistory();
}

void TextAssistant::trimConversationHistory() {
    if (conversation_history_.size() > static_cast<size_t>(assistant_config_.max_conversation_history * 2)) {
        // Keep only the most recent messages
        int keep_count = assistant_config_.max_conversation_history;
        conversation_history_.erase(
            conversation_history_.begin(),
            conversation_history_.end() - keep_count
        );
    }
}

void TextAssistant::fireEvent(AssistantEvent event, const std::string& data) {
    if (event_callback_) {
        event_callback_(event, data);
    }
}



void TextAssistant::handleError(const std::string& error_message) {
    LOG_ERROR(error_message);
    setState(AssistantState::ERROR);
    fireEvent(AssistantEvent::ERROR_OCCURRED, error_message);
}

std::string TextAssistant::generateConversationTitle(const std::string& first_message) {
    // Simple title generation - take first few words
    std::string title = first_message;
    if (title.length() > 50) {
        title = title.substr(0, 47) + "...";
    }

    // Remove newlines and extra spaces
    std::replace(title.begin(), title.end(), '\n', ' ');
    std::replace(title.begin(), title.end(), '\r', ' ');

    // Collapse multiple spaces
    std::regex multiple_spaces("\\s+");
    title = std::regex_replace(title, multiple_spaces, " ");

    return title.empty() ? "New Conversation" : title;
}

bool TextAssistant::updateConversationInDatabase() {
    if (!database_ || current_conversation_id_.empty()) {
        return false;
    }

    // Update conversation title if it's empty and we have messages
    if (!conversation_history_.empty()) {
        auto conversation = database_->getConversation(current_conversation_id_);
        if (conversation.has_value() && conversation->title.empty()) {
            std::string title = generateConversationTitle(conversation_history_[0].content);
            database_->updateConversationTitle(current_conversation_id_, title);
        }
    }

    return true;
}

} // namespace AITextAssistant
