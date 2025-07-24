#include "database/conversation_db.h"
#include "utils/logger.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace AITextAssistant {

ConversationDB::ConversationDB(const std::string& db_path) 
    : db_(nullptr), db_path_(db_path), in_transaction_(false) {
}

ConversationDB::~ConversationDB() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool ConversationDB::initialize() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    // Enable foreign keys
    executeSQL("PRAGMA foreign_keys = ON;");
    
    // Create tables and indexes
    if (!createTables() || !createIndexes()) {
        return false;
    }
    
    LOG_INFO("Database initialized successfully: " + db_path_);
    return true;
}

bool ConversationDB::createTables() {
    const std::string create_conversations_table = R"(
        CREATE TABLE IF NOT EXISTS conversations (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL DEFAULT '',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );
    )";
    
    const std::string create_messages_table = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id TEXT PRIMARY KEY,
            conversation_id TEXT NOT NULL,
            role TEXT NOT NULL CHECK(role IN ('user', 'assistant', 'system')),
            content TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE
        );
    )";
    
    return executeSQL(create_conversations_table) && executeSQL(create_messages_table);
}

bool ConversationDB::createIndexes() {
    const std::vector<std::string> indexes = {
        "CREATE INDEX IF NOT EXISTS idx_conversations_created_at ON conversations(created_at);",
        "CREATE INDEX IF NOT EXISTS idx_conversations_updated_at ON conversations(updated_at);",
        "CREATE INDEX IF NOT EXISTS idx_messages_conversation_id ON messages(conversation_id);",
        "CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_messages_role ON messages(role);",
        "CREATE INDEX IF NOT EXISTS idx_messages_content_fts ON messages(content);"
    };
    
    for (const auto& index : indexes) {
        if (!executeSQL(index)) {
            return false;
        }
    }
    
    return true;
}

std::string ConversationDB::createConversation(const std::string& title) {
    std::string conversation_id = generateId();
    std::string timestamp = formatTimestamp(std::chrono::system_clock::now());
    
    const std::string sql = R"(
        INSERT INTO conversations (id, title, created_at, updated_at)
        VALUES (?, ?, ?, ?);
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return "";
    
    sqlite3_bind_text(stmt, 1, conversation_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, timestamp.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, timestamp.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);
    
    if (rc == SQLITE_DONE) {
        LOG_INFO("Created conversation: " + conversation_id);
        return conversation_id;
    } else {
        logSQLError("createConversation");
        return "";
    }
}

bool ConversationDB::deleteConversation(const ConversationId& conversation_id) {
    const std::string sql = "DELETE FROM conversations WHERE id = ?;";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return false;
    
    sqlite3_bind_text(stmt, 1, conversation_id.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);
    
    if (rc == SQLITE_DONE) {
        LOG_INFO("Deleted conversation: " + conversation_id);
        return true;
    } else {
        logSQLError("deleteConversation");
        return false;
    }
}

bool ConversationDB::updateConversationTitle(const ConversationId& conversation_id, const std::string& title) {
    const std::string sql = R"(
        UPDATE conversations 
        SET title = ?, updated_at = ? 
        WHERE id = ?;
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return false;
    
    std::string timestamp = formatTimestamp(std::chrono::system_clock::now());
    
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, timestamp.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, conversation_id.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);
    
    return rc == SQLITE_DONE;
}

MessageId ConversationDB::addMessage(const ConversationId& conversation_id, const Message& message) {
    std::string message_id = generateId();
    std::string timestamp = formatTimestamp(message.timestamp);
    
    const std::string sql = R"(
        INSERT INTO messages (id, conversation_id, role, content, timestamp)
        VALUES (?, ?, ?, ?, ?);
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return "";
    
    sqlite3_bind_text(stmt, 1, message_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, conversation_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message.role.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, message.content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, timestamp.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);
    
    if (rc == SQLITE_DONE) {
        // Update conversation timestamp
        const std::string update_sql = "UPDATE conversations SET updated_at = ? WHERE id = ?;";
        sqlite3_stmt* update_stmt = prepareStatement(update_sql);
        if (update_stmt) {
            sqlite3_bind_text(update_stmt, 1, timestamp.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(update_stmt, 2, conversation_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(update_stmt);
            finalizeStatement(update_stmt);
        }
        
        LOG_DEBUG("Added message: " + message_id + " to conversation: " + conversation_id);
        return message_id;
    } else {
        logSQLError("addMessage");
        return "";
    }
}

bool ConversationDB::updateMessage(const MessageId& message_id, const std::string& content) {
    const std::string sql = "UPDATE messages SET content = ? WHERE id = ?;";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return false;
    
    sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, message_id.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);
    
    return rc == SQLITE_DONE;
}

bool ConversationDB::deleteMessage(const MessageId& message_id) {
    const std::string sql = "DELETE FROM messages WHERE id = ?;";

    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, message_id.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

std::optional<Conversation> ConversationDB::getConversation(const ConversationId& conversation_id) {
    const std::string sql = R"(
        SELECT id, title, created_at, updated_at
        FROM conversations
        WHERE id = ?;
    )";

    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return std::nullopt;

    sqlite3_bind_text(stmt, 1, conversation_id.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        Conversation conversation = parseConversationFromRow(stmt);
        finalizeStatement(stmt);

        // Load messages
        conversation.messages = getConversationMessages(conversation_id);
        return conversation;
    }

    finalizeStatement(stmt);
    return std::nullopt;
}

std::vector<Conversation> ConversationDB::getAllConversations() {
    const std::string sql = R"(
        SELECT id, title, created_at, updated_at
        FROM conversations
        ORDER BY updated_at DESC;
    )";

    std::vector<Conversation> conversations;
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return conversations;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        conversations.push_back(parseConversationFromRow(stmt));
    }

    finalizeStatement(stmt);
    return conversations;
}

std::vector<Conversation> ConversationDB::getRecentConversations(int limit) {
    const std::string sql = R"(
        SELECT id, title, created_at, updated_at
        FROM conversations
        ORDER BY updated_at DESC
        LIMIT ?;
    )";

    std::vector<Conversation> conversations;
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return conversations;

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        conversations.push_back(parseConversationFromRow(stmt));
    }

    finalizeStatement(stmt);
    return conversations;
}

std::vector<Message> ConversationDB::getConversationMessages(const ConversationId& conversation_id, int limit) {
    std::string sql = R"(
        SELECT id, conversation_id, role, content, timestamp
        FROM messages
        WHERE conversation_id = ?
        ORDER BY timestamp ASC
    )";

    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    std::vector<Message> messages;
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return messages;

    sqlite3_bind_text(stmt, 1, conversation_id.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        messages.push_back(parseMessageFromRow(stmt));
    }

    finalizeStatement(stmt);
    return messages;
}

std::vector<Message> ConversationDB::getRecentMessages(const ConversationId& conversation_id, int limit) {
    const std::string sql = R"(
        SELECT id, conversation_id, role, content, timestamp
        FROM messages
        WHERE conversation_id = ?
        ORDER BY timestamp DESC
        LIMIT ?;
    )";

    std::vector<Message> messages;
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return messages;

    sqlite3_bind_text(stmt, 1, conversation_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        messages.push_back(parseMessageFromRow(stmt));
    }

    finalizeStatement(stmt);

    // Reverse to get chronological order
    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<Conversation> ConversationDB::searchConversations(const std::string& query) {
    const std::string sql = R"(
        SELECT DISTINCT c.id, c.title, c.created_at, c.updated_at
        FROM conversations c
        LEFT JOIN messages m ON c.id = m.conversation_id
        WHERE c.title LIKE ? OR m.content LIKE ?
        ORDER BY c.updated_at DESC;
    )";

    std::vector<Conversation> conversations;
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return conversations;

    std::string search_pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, search_pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, search_pattern.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        conversations.push_back(parseConversationFromRow(stmt));
    }

    finalizeStatement(stmt);
    return conversations;
}

std::vector<Message> ConversationDB::searchMessages(const std::string& query, const ConversationId& conversation_id) {
    std::string sql = R"(
        SELECT id, conversation_id, role, content, timestamp
        FROM messages
        WHERE content LIKE ?
    )";

    if (!conversation_id.empty()) {
        sql += " AND conversation_id = ?";
    }

    sql += " ORDER BY timestamp DESC;";

    std::vector<Message> messages;
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return messages;

    std::string search_pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, search_pattern.c_str(), -1, SQLITE_STATIC);

    if (!conversation_id.empty()) {
        sqlite3_bind_text(stmt, 2, conversation_id.c_str(), -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        messages.push_back(parseMessageFromRow(stmt));
    }

    finalizeStatement(stmt);
    return messages;
}

int ConversationDB::getConversationCount() {
    const std::string sql = "SELECT COUNT(*) FROM conversations;";

    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return 0;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    finalizeStatement(stmt);
    return count;
}

int ConversationDB::getMessageCount(const ConversationId& conversation_id) {
    std::string sql = "SELECT COUNT(*) FROM messages";
    if (!conversation_id.empty()) {
        sql += " WHERE conversation_id = ?";
    }

    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) return 0;

    if (!conversation_id.empty()) {
        sqlite3_bind_text(stmt, 1, conversation_id.c_str(), -1, SQLITE_STATIC);
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    finalizeStatement(stmt);
    return count;
}

bool ConversationDB::vacuum() {
    return executeSQL("VACUUM;");
}

bool ConversationDB::backup(const std::string& backup_path) {
    sqlite3* backup_db;
    int rc = sqlite3_open(backup_path.c_str(), &backup_db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot create backup database: " + std::string(sqlite3_errmsg(backup_db)));
        return false;
    }

    sqlite3_backup* backup_handle = sqlite3_backup_init(backup_db, "main", db_, "main");
    if (!backup_handle) {
        LOG_ERROR("Cannot initialize backup: " + std::string(sqlite3_errmsg(backup_db)));
        sqlite3_close(backup_db);
        return false;
    }

    rc = sqlite3_backup_step(backup_handle, -1);
    sqlite3_backup_finish(backup_handle);
    sqlite3_close(backup_db);

    if (rc == SQLITE_DONE) {
        LOG_INFO("Database backup completed: " + backup_path);
        return true;
    } else {
        LOG_ERROR("Backup failed: " + std::string(sqlite3_errstr(rc)));
        return false;
    }
}

bool ConversationDB::restore(const std::string& backup_path) {
    sqlite3* backup_db;
    int rc = sqlite3_open(backup_path.c_str(), &backup_db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot open backup database: " + std::string(sqlite3_errmsg(backup_db)));
        return false;
    }

    sqlite3_backup* backup_handle = sqlite3_backup_init(db_, "main", backup_db, "main");
    if (!backup_handle) {
        LOG_ERROR("Cannot initialize restore: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_close(backup_db);
        return false;
    }

    rc = sqlite3_backup_step(backup_handle, -1);
    sqlite3_backup_finish(backup_handle);
    sqlite3_close(backup_db);

    if (rc == SQLITE_DONE) {
        LOG_INFO("Database restore completed from: " + backup_path);
        return true;
    } else {
        LOG_ERROR("Restore failed: " + std::string(sqlite3_errstr(rc)));
        return false;
    }
}

bool ConversationDB::beginTransaction() {
    if (in_transaction_) {
        LOG_WARNING("Transaction already in progress");
        return false;
    }

    if (executeSQL("BEGIN TRANSACTION;")) {
        in_transaction_ = true;
        return true;
    }
    return false;
}

bool ConversationDB::commitTransaction() {
    if (!in_transaction_) {
        LOG_WARNING("No transaction in progress");
        return false;
    }

    if (executeSQL("COMMIT;")) {
        in_transaction_ = false;
        return true;
    }
    return false;
}

bool ConversationDB::rollbackTransaction() {
    if (!in_transaction_) {
        LOG_WARNING("No transaction in progress");
        return false;
    }

    if (executeSQL("ROLLBACK;")) {
        in_transaction_ = false;
        return true;
    }
    return false;
}

std::string ConversationDB::generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << '-';
        }
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

Timestamp ConversationDB::parseTimestamp(const std::string& timestamp_str) {
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return time_point;
}

std::string ConversationDB::formatTimestamp(const Timestamp& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool ConversationDB::executeSQL(const std::string& sql) {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

    if (rc != SQLITE_OK) {
        LOG_ERROR("SQL error: " + std::string(error_msg));
        sqlite3_free(error_msg);
        return false;
    }

    return true;
}

sqlite3_stmt* ConversationDB::prepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return nullptr;
    }

    return stmt;
}

void ConversationDB::finalizeStatement(sqlite3_stmt* stmt) {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

Message ConversationDB::parseMessageFromRow(sqlite3_stmt* stmt) {
    Message message;
    message.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    message.conversation_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    message.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    message.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    const char* timestamp_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    message.timestamp = parseTimestamp(timestamp_str);

    return message;
}

Conversation ConversationDB::parseConversationFromRow(sqlite3_stmt* stmt) {
    Conversation conversation;
    conversation.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    conversation.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

    const char* created_at_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const char* updated_at_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    conversation.created_at = parseTimestamp(created_at_str);
    conversation.updated_at = parseTimestamp(updated_at_str);

    return conversation;
}

void ConversationDB::logSQLError(const std::string& operation) {
    LOG_ERROR("SQL error in " + operation + ": " + std::string(sqlite3_errmsg(db_)));
}

} // namespace AITextAssistant
