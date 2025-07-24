#include <gtest/gtest.h>
#include "database/conversation_db.h"
#include <filesystem>
#include <chrono>
#include <thread>

using namespace AITextAssistant;

class ConversationDBTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path = "test_conversations.db";
        
        // Clean up any existing test database
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
        
        db = std::make_unique<ConversationDB>(test_db_path);
        ASSERT_TRUE(db->initialize());
    }
    
    void TearDown() override {
        db.reset(); // Close database connection
        
        // Clean up test database
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
    }
    
    std::unique_ptr<ConversationDB> db;
    std::string test_db_path;
};

TEST_F(ConversationDBTest, DatabaseInitialization) {
    EXPECT_TRUE(db->isInitialized());
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
}

TEST_F(ConversationDBTest, CreateConversation) {
    std::string conversation_id = db->createConversation("Test Conversation");
    
    EXPECT_FALSE(conversation_id.empty());
    EXPECT_EQ(db->getConversationCount(), 1);
    
    // Verify conversation exists
    auto conversation = db->getConversation(conversation_id);
    ASSERT_TRUE(conversation.has_value());
    EXPECT_EQ(conversation->title, "Test Conversation");
    EXPECT_EQ(conversation->id, conversation_id);
}

TEST_F(ConversationDBTest, CreateConversationWithoutTitle) {
    std::string conversation_id = db->createConversation();
    
    EXPECT_FALSE(conversation_id.empty());
    
    auto conversation = db->getConversation(conversation_id);
    ASSERT_TRUE(conversation.has_value());
    EXPECT_EQ(conversation->title, ""); // Empty title
}

TEST_F(ConversationDBTest, DeleteConversation) {
    std::string conversation_id = db->createConversation("To be deleted");
    EXPECT_EQ(db->getConversationCount(), 1);
    
    EXPECT_TRUE(db->deleteConversation(conversation_id));
    EXPECT_EQ(db->getConversationCount(), 0);
    
    // Verify conversation no longer exists
    auto conversation = db->getConversation(conversation_id);
    EXPECT_FALSE(conversation.has_value());
}

TEST_F(ConversationDBTest, UpdateConversationTitle) {
    std::string conversation_id = db->createConversation("Original Title");
    
    EXPECT_TRUE(db->updateConversationTitle(conversation_id, "Updated Title"));
    
    auto conversation = db->getConversation(conversation_id);
    ASSERT_TRUE(conversation.has_value());
    EXPECT_EQ(conversation->title, "Updated Title");
}

TEST_F(ConversationDBTest, AddMessage) {
    std::string conversation_id = db->createConversation("Test Conversation");
    
    Message message("user", "Hello, world!");
    std::string message_id = db->addMessage(conversation_id, message);
    
    EXPECT_FALSE(message_id.empty());
    EXPECT_EQ(db->getMessageCount(conversation_id), 1);
    
    // Verify message was added to conversation
    auto conversation = db->getConversation(conversation_id);
    ASSERT_TRUE(conversation.has_value());
    ASSERT_EQ(conversation->messages.size(), 1);
    EXPECT_EQ(conversation->messages[0].role, "user");
    EXPECT_EQ(conversation->messages[0].content, "Hello, world!");
}

TEST_F(ConversationDBTest, AddMultipleMessages) {
    std::string conversation_id = db->createConversation("Multi-message Conversation");
    
    std::vector<Message> messages = {
        {"user", "Hello"},
        {"assistant", "Hi there!"},
        {"user", "How are you?"},
        {"assistant", "I'm doing well, thank you!"}
    };
    
    for (const auto& msg : messages) {
        std::string msg_id = db->addMessage(conversation_id, msg);
        EXPECT_FALSE(msg_id.empty());
    }
    
    EXPECT_EQ(db->getMessageCount(conversation_id), 4);
    
    auto conversation = db->getConversation(conversation_id);
    ASSERT_TRUE(conversation.has_value());
    ASSERT_EQ(conversation->messages.size(), 4);
    
    // Verify message order
    for (size_t i = 0; i < messages.size(); ++i) {
        EXPECT_EQ(conversation->messages[i].role, messages[i].role);
        EXPECT_EQ(conversation->messages[i].content, messages[i].content);
    }
}

TEST_F(ConversationDBTest, GetConversationMessages) {
    std::string conversation_id = db->createConversation("Message Test");
    
    // Add some messages
    db->addMessage(conversation_id, {"user", "Message 1"});
    db->addMessage(conversation_id, {"assistant", "Response 1"});
    db->addMessage(conversation_id, {"user", "Message 2"});
    
    auto messages = db->getConversationMessages(conversation_id);
    EXPECT_EQ(messages.size(), 3);
    
    // Test with limit
    auto limited_messages = db->getConversationMessages(conversation_id, 2);
    EXPECT_EQ(limited_messages.size(), 2);
}

TEST_F(ConversationDBTest, GetRecentMessages) {
    std::string conversation_id = db->createConversation("Recent Messages Test");
    
    // Add messages with slight delays to ensure different timestamps
    for (int i = 1; i <= 5; ++i) {
        db->addMessage(conversation_id, {"user", "Message " + std::to_string(i)});
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto recent_messages = db->getRecentMessages(conversation_id, 3);
    EXPECT_EQ(recent_messages.size(), 3);
    
    // Should be in chronological order (oldest first)
    EXPECT_EQ(recent_messages[0].content, "Message 3");
    EXPECT_EQ(recent_messages[1].content, "Message 4");
    EXPECT_EQ(recent_messages[2].content, "Message 5");
}

TEST_F(ConversationDBTest, UpdateMessage) {
    std::string conversation_id = db->createConversation("Update Test");
    Message message("user", "Original content");
    std::string message_id = db->addMessage(conversation_id, message);
    
    EXPECT_TRUE(db->updateMessage(message_id, "Updated content"));
    
    auto conversation = db->getConversation(conversation_id);
    ASSERT_TRUE(conversation.has_value());
    ASSERT_EQ(conversation->messages.size(), 1);
    EXPECT_EQ(conversation->messages[0].content, "Updated content");
}

TEST_F(ConversationDBTest, DeleteMessage) {
    std::string conversation_id = db->createConversation("Delete Message Test");
    Message message("user", "To be deleted");
    std::string message_id = db->addMessage(conversation_id, message);
    
    EXPECT_EQ(db->getMessageCount(conversation_id), 1);
    
    EXPECT_TRUE(db->deleteMessage(message_id));
    EXPECT_EQ(db->getMessageCount(conversation_id), 0);
}

TEST_F(ConversationDBTest, GetAllConversations) {
    // Create multiple conversations
    db->createConversation("Conversation 1");
    db->createConversation("Conversation 2");
    db->createConversation("Conversation 3");
    
    auto conversations = db->getAllConversations();
    EXPECT_EQ(conversations.size(), 3);
}

TEST_F(ConversationDBTest, GetRecentConversations) {
    // Create conversations with delays to ensure different timestamps
    for (int i = 1; i <= 5; ++i) {
        db->createConversation("Conversation " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto recent = db->getRecentConversations(3);
    EXPECT_EQ(recent.size(), 3);
    
    // Should be ordered by most recent first
    EXPECT_EQ(recent[0].title, "Conversation 5");
    EXPECT_EQ(recent[1].title, "Conversation 4");
    EXPECT_EQ(recent[2].title, "Conversation 3");
}

TEST_F(ConversationDBTest, SearchConversations) {
    db->createConversation("Important Meeting Notes");
    db->createConversation("Shopping List");
    db->createConversation("Meeting Summary");
    
    auto results = db->searchConversations("meeting");
    EXPECT_EQ(results.size(), 2);
    
    // Verify both conversations with "meeting" are found
    bool found_notes = false, found_summary = false;
    for (const auto& conv : results) {
        if (conv.title == "Important Meeting Notes") found_notes = true;
        if (conv.title == "Meeting Summary") found_summary = true;
    }
    EXPECT_TRUE(found_notes);
    EXPECT_TRUE(found_summary);
}

TEST_F(ConversationDBTest, SearchMessages) {
    std::string conv1 = db->createConversation("Conversation 1");
    std::string conv2 = db->createConversation("Conversation 2");
    
    db->addMessage(conv1, {"user", "I love programming"});
    db->addMessage(conv1, {"assistant", "That's great!"});
    db->addMessage(conv2, {"user", "Programming is fun"});
    db->addMessage(conv2, {"assistant", "I agree!"});
    
    // Search all messages
    auto all_results = db->searchMessages("programming");
    EXPECT_EQ(all_results.size(), 2);
    
    // Search within specific conversation
    auto conv1_results = db->searchMessages("programming", conv1);
    EXPECT_EQ(conv1_results.size(), 1);
    EXPECT_EQ(conv1_results[0].content, "I love programming");
}

TEST_F(ConversationDBTest, TransactionSupport) {
    EXPECT_TRUE(db->beginTransaction());
    
    std::string conv_id = db->createConversation("Transaction Test");
    EXPECT_FALSE(conv_id.empty());
    
    EXPECT_TRUE(db->commitTransaction());
    
    // Verify conversation was committed
    auto conversation = db->getConversation(conv_id);
    EXPECT_TRUE(conversation.has_value());
}

TEST_F(ConversationDBTest, TransactionRollback) {
    EXPECT_TRUE(db->beginTransaction());
    
    std::string conv_id = db->createConversation("Rollback Test");
    EXPECT_FALSE(conv_id.empty());
    
    EXPECT_TRUE(db->rollbackTransaction());
    
    // Verify conversation was rolled back
    auto conversation = db->getConversation(conv_id);
    EXPECT_FALSE(conversation.has_value());
}

TEST_F(ConversationDBTest, DatabaseVacuum) {
    // Add some data
    std::string conv_id = db->createConversation("Vacuum Test");
    db->addMessage(conv_id, {"user", "Test message"});
    
    // Delete the data
    db->deleteConversation(conv_id);
    
    // Vacuum should succeed
    EXPECT_TRUE(db->vacuum());
}
