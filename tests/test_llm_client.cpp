#include <gtest/gtest.h>
#include "llm/llm_client.h"
#include <nlohmann/json.hpp>

using namespace AITextAssistant;

class LLMClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test configuration
        test_config.provider = "openai";
        test_config.api_endpoint = "https://api.openai.com/v1/chat/completions";
        test_config.api_key = "test-key";
        test_config.model_name = "gpt-3.5-turbo";
        test_config.temperature = 0.7;
        test_config.max_tokens = 1000;
        test_config.headers["Content-Type"] = "application/json";
    }
    
    LLMConfig test_config;
};

TEST_F(LLMClientTest, ClientCreation) {
    // Test OpenAI client creation
    auto openai_client = LLMClient::createClient(test_config);
    EXPECT_NE(openai_client, nullptr);
    
    // Test Anthropic client creation
    test_config.provider = "anthropic";
    auto anthropic_client = LLMClient::createClient(test_config);
    EXPECT_NE(anthropic_client, nullptr);
    
    // Test custom client creation
    test_config.provider = "custom";
    auto custom_client = LLMClient::createClient(test_config);
    EXPECT_NE(custom_client, nullptr);
}

TEST_F(LLMClientTest, ConfigurationUpdate) {
    auto client = LLMClient::createClient(test_config);
    
    // Update configuration
    LLMConfig new_config = test_config;
    new_config.temperature = 0.5;
    new_config.max_tokens = 2000;
    
    client->updateConfig(new_config);
    
    const auto& updated_config = client->getConfig();
    EXPECT_EQ(updated_config.temperature, 0.5);
    EXPECT_EQ(updated_config.max_tokens, 2000);
}

class OpenAIClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.provider = "openai";
        config.api_endpoint = "https://api.openai.com/v1/chat/completions";
        config.api_key = "test-key";
        config.model_name = "gpt-3.5-turbo";
        config.temperature = 0.7;
        config.max_tokens = 1000;
        config.headers["Content-Type"] = "application/json";
        
        client = std::make_unique<OpenAIClient>(config);
    }
    
    LLMConfig config;
    std::unique_ptr<OpenAIClient> client;
};

TEST_F(OpenAIClientTest, RequestPayloadBuilding) {
    std::vector<Message> messages = {
        {"system", "You are a helpful assistant."},
        {"user", "Hello, how are you?"}
    };
    
    // Access protected method through inheritance (for testing)
    class TestableOpenAIClient : public OpenAIClient {
    public:
        TestableOpenAIClient(const LLMConfig& config) : OpenAIClient(config) {}
        
        std::string testBuildRequestPayload(const std::vector<Message>& messages) {
            return buildRequestPayload(messages);
        }
    };
    
    TestableOpenAIClient testable_client(config);
    std::string payload = testable_client.testBuildRequestPayload(messages);
    
    // Parse the JSON payload
    nlohmann::json json_payload = nlohmann::json::parse(payload);
    
    EXPECT_EQ(json_payload["model"], "gpt-3.5-turbo");
    EXPECT_EQ(json_payload["temperature"], 0.7);
    EXPECT_EQ(json_payload["max_tokens"], 1000);
    EXPECT_TRUE(json_payload.contains("messages"));
    EXPECT_EQ(json_payload["messages"].size(), 2);
    EXPECT_EQ(json_payload["messages"][0]["role"], "system");
    EXPECT_EQ(json_payload["messages"][1]["role"], "user");
}

TEST_F(OpenAIClientTest, ResponseParsing) {
    class TestableOpenAIClient : public OpenAIClient {
    public:
        TestableOpenAIClient(const LLMConfig& config) : OpenAIClient(config) {}
        
        LLMResponse testParseResponse(const HTTPResponse& http_response) {
            return parseResponse(http_response);
        }
    };
    
    TestableOpenAIClient testable_client(config);
    
    // Test successful response
    HTTPResponse http_response;
    http_response.success = true;
    http_response.status_code = 200;
    http_response.body = R"({
        "choices": [
            {
                "message": {
                    "content": "Hello! I'm doing well, thank you for asking."
                }
            }
        ],
        "usage": {
            "prompt_tokens": 20,
            "completion_tokens": 15,
            "total_tokens": 35
        }
    })";
    
    LLMResponse response = testable_client.testParseResponse(http_response);
    
    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.content, "Hello! I'm doing well, thank you for asking.");
    EXPECT_EQ(response.metadata["prompt_tokens"], "20");
    EXPECT_EQ(response.metadata["completion_tokens"], "15");
    EXPECT_EQ(response.metadata["total_tokens"], "35");
}

TEST_F(OpenAIClientTest, ErrorResponseParsing) {
    class TestableOpenAIClient : public OpenAIClient {
    public:
        TestableOpenAIClient(const LLMConfig& config) : OpenAIClient(config) {}
        
        LLMResponse testParseResponse(const HTTPResponse& http_response) {
            return parseResponse(http_response);
        }
    };
    
    TestableOpenAIClient testable_client(config);
    
    // Test error response
    HTTPResponse http_response;
    http_response.success = false;
    http_response.status_code = 400;
    http_response.body = R"({
        "error": {
            "message": "Invalid API key provided",
            "type": "invalid_request_error"
        }
    })";
    
    LLMResponse response = testable_client.testParseResponse(http_response);
    
    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error_message, "Invalid API key provided");
    EXPECT_EQ(response.status_code, 400);
}

class AnthropicClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.provider = "anthropic";
        config.api_endpoint = "https://api.anthropic.com/v1/messages";
        config.api_key = "test-key";
        config.model_name = "claude-3-sonnet-20240229";
        config.temperature = 0.7;
        config.max_tokens = 1000;
        config.headers["Content-Type"] = "application/json";
        
        client = std::make_unique<AnthropicClient>(config);
    }
    
    LLMConfig config;
    std::unique_ptr<AnthropicClient> client;
};

TEST_F(AnthropicClientTest, RequestPayloadBuilding) {
    std::vector<Message> messages = {
        {"system", "You are a helpful assistant."},
        {"user", "Hello, how are you?"}
    };
    
    class TestableAnthropicClient : public AnthropicClient {
    public:
        TestableAnthropicClient(const LLMConfig& config) : AnthropicClient(config) {}
        
        std::string testBuildRequestPayload(const std::vector<Message>& messages) {
            return buildRequestPayload(messages);
        }
    };
    
    TestableAnthropicClient testable_client(config);
    std::string payload = testable_client.testBuildRequestPayload(messages);
    
    nlohmann::json json_payload = nlohmann::json::parse(payload);
    
    EXPECT_EQ(json_payload["model"], "claude-3-sonnet-20240229");
    EXPECT_EQ(json_payload["max_tokens"], 1000);
    EXPECT_TRUE(json_payload.contains("system"));
    EXPECT_EQ(json_payload["system"], "You are a helpful assistant.");
    EXPECT_TRUE(json_payload.contains("messages"));
    EXPECT_EQ(json_payload["messages"].size(), 1); // System message is separate
    EXPECT_EQ(json_payload["messages"][0]["role"], "user");
}

class HTTPClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_client = std::make_unique<HTTPClient>();
    }
    
    std::unique_ptr<HTTPClient> http_client;
};

TEST_F(HTTPClientTest, Initialization) {
    EXPECT_NE(http_client, nullptr);
}

TEST_F(HTTPClientTest, TimeoutSetting) {
    // This test just verifies the method doesn't crash
    http_client->setTimeout(30);
    // No assertion needed - if it doesn't crash, it passes
}

TEST_F(HTTPClientTest, UserAgentSetting) {
    // This test just verifies the method doesn't crash
    http_client->setUserAgent("TestAgent/1.0");
    // No assertion needed - if it doesn't crash, it passes
}

// Note: We don't test actual HTTP requests in unit tests
// Those would be integration tests requiring network access
