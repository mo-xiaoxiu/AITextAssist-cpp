#pragma once

#include "common/types.h"
#include <curl/curl.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace AITextAssistant {

// HTTP Response structure
struct HTTPResponse {
    long status_code;
    std::string body;
    std::map<std::string, std::string> headers;
    bool success;
    std::string error_message;
};

// HTTP Client for API calls
class HTTPClient {
public:
    HTTPClient();
    ~HTTPClient();
    
    HTTPResponse post(const std::string& url, 
                     const std::string& data,
                     const std::map<std::string, std::string>& headers = {});
    
    HTTPResponse get(const std::string& url,
                    const std::map<std::string, std::string>& headers = {});
    
    void setTimeout(long timeout_seconds);
    void setUserAgent(const std::string& user_agent);

private:
    CURL* curl_;
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, std::map<std::string, std::string>* userp);
    
    void setupCommonOptions();
    struct curl_slist* buildHeaders(const std::map<std::string, std::string>& headers);
};

// LLM Client interface
class LLMClient {
public:
    explicit LLMClient(const LLMConfig& config);
    virtual ~LLMClient() = default;
    
    // Main chat completion method
    virtual LLMResponse chatCompletion(const std::vector<Message>& messages);
    
    // Stream chat completion (for real-time responses)
    virtual void streamChatCompletion(const std::vector<Message>& messages,
                                    std::function<void(const std::string&)> callback);
    
    // Configuration management
    void updateConfig(const LLMConfig& config);
    const LLMConfig& getConfig() const { return config_; }
    
    // Provider-specific implementations
    static std::unique_ptr<LLMClient> createClient(const LLMConfig& config);

protected:
    LLMConfig config_;
    std::unique_ptr<HTTPClient> http_client_;
    
    // Abstract methods for different providers
    virtual std::string buildRequestPayload(const std::vector<Message>& messages) = 0;
    virtual LLMResponse parseResponse(const HTTPResponse& http_response) = 0;
    virtual std::map<std::string, std::string> buildHeaders() = 0;
};

// OpenAI API client
class OpenAIClient : public LLMClient {
public:
    explicit OpenAIClient(const LLMConfig& config);
    
protected:
    std::string buildRequestPayload(const std::vector<Message>& messages) override;
    LLMResponse parseResponse(const HTTPResponse& http_response) override;
    std::map<std::string, std::string> buildHeaders() override;
};

// Anthropic Claude API client
class AnthropicClient : public LLMClient {
public:
    explicit AnthropicClient(const LLMConfig& config);
    
protected:
    std::string buildRequestPayload(const std::vector<Message>& messages) override;
    LLMResponse parseResponse(const HTTPResponse& http_response) override;
    std::map<std::string, std::string> buildHeaders() override;
};

// Generic/Custom API client
class CustomClient : public LLMClient {
public:
    explicit CustomClient(const LLMConfig& config);
    
protected:
    std::string buildRequestPayload(const std::vector<Message>& messages) override;
    LLMResponse parseResponse(const HTTPResponse& http_response) override;
    std::map<std::string, std::string> buildHeaders() override;
};

} // namespace AITextAssistant
