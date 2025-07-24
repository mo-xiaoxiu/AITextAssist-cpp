#pragma once

#include "common/types.h"
#include <sqlite3.h>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace AITextAssistant {

class ConversationDB {
public:
    explicit ConversationDB(const std::string& db_path);
    ~ConversationDB();
    
    // Database initialization
    bool initialize();
    bool isInitialized() const { return db_ != nullptr; }
    
    // Conversation management
    std::string createConversation(const std::string& title = "");
    bool deleteConversation(const ConversationId& conversation_id);
    bool updateConversationTitle(const ConversationId& conversation_id, const std::string& title);
    
    // Message operations
    MessageId addMessage(const ConversationId& conversation_id, const Message& message);
    bool updateMessage(const MessageId& message_id, const std::string& content);
    bool deleteMessage(const MessageId& message_id);
    
    // Retrieval operations
    std::optional<Conversation> getConversation(const ConversationId& conversation_id);
    std::vector<Conversation> getAllConversations();
    std::vector<Conversation> getRecentConversations(int limit = 10);
    std::vector<Message> getConversationMessages(const ConversationId& conversation_id, int limit = -1);
    std::vector<Message> getRecentMessages(const ConversationId& conversation_id, int limit = 10);
    
    // Search operations
    std::vector<Conversation> searchConversations(const std::string& query);
    std::vector<Message> searchMessages(const std::string& query, const ConversationId& conversation_id = "");
    
    // Statistics
    int getConversationCount();
    int getMessageCount(const ConversationId& conversation_id = "");
    
    // Maintenance
    bool vacuum();
    bool backup(const std::string& backup_path);
    bool restore(const std::string& backup_path);
    
    // Transaction support
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

private:
    sqlite3* db_;
    std::string db_path_;
    bool in_transaction_;
    
    // Database schema creation
    bool createTables();
    bool createIndexes();
    
    // Helper methods
    std::string generateId();
    Timestamp parseTimestamp(const std::string& timestamp_str);
    std::string formatTimestamp(const Timestamp& timestamp);
    
    // SQL execution helpers
    bool executeSQL(const std::string& sql);
    sqlite3_stmt* prepareStatement(const std::string& sql);
    void finalizeStatement(sqlite3_stmt* stmt);
    
    // Result parsing helpers
    Message parseMessageFromRow(sqlite3_stmt* stmt);
    Conversation parseConversationFromRow(sqlite3_stmt* stmt);
    
    // Error handling
    void logSQLError(const std::string& operation);
};

} // namespace AITextAssistant
