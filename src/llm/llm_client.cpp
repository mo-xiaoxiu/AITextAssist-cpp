#include "llm/llm_client.h"
#include "utils/logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

namespace AITextAssistant {

// HTTPClient implementation
HTTPClient::HTTPClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
    if (curl_) {
        setupCommonOptions();
    }
}

HTTPClient::~HTTPClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
    curl_global_cleanup();
}

void HTTPClient::setupCommonOptions() {
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, "AITextAssistant/1.0");
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
}

HTTPResponse HTTPClient::post(const std::string& url, 
                             const std::string& data,
                             const std::map<std::string, std::string>& headers) {
    HTTPResponse response;
    
    if (!curl_) {
        response.success = false;
        response.error_message = "CURL not initialized";
        return response;
    }
    
    std::string response_body;
    std::map<std::string, std::string> response_headers;
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &response_headers);
    
    struct curl_slist* header_list = buildHeaders(headers);
    if (header_list) {
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
    }
    
    CURLcode res = curl_easy_perform(curl_);
    
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    
    if (res != CURLE_OK) {
        response.success = false;
        response.error_message = curl_easy_strerror(res);
        return response;
    }
    
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = response_body;
    response.headers = response_headers;
    response.success = (response.status_code >= 200 && response.status_code < 300);
    
    return response;
}

HTTPResponse HTTPClient::get(const std::string& url,
                            const std::map<std::string, std::string>& headers) {
    HTTPResponse response;
    
    if (!curl_) {
        response.success = false;
        response.error_message = "CURL not initialized";
        return response;
    }
    
    std::string response_body;
    std::map<std::string, std::string> response_headers;
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &response_headers);
    
    struct curl_slist* header_list = buildHeaders(headers);
    if (header_list) {
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
    }
    
    CURLcode res = curl_easy_perform(curl_);
    
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    
    if (res != CURLE_OK) {
        response.success = false;
        response.error_message = curl_easy_strerror(res);
        return response;
    }
    
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = response_body;
    response.headers = response_headers;
    response.success = (response.status_code >= 200 && response.status_code < 300);
    
    return response;
}

void HTTPClient::setTimeout(long timeout_seconds) {
    if (curl_) {
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_seconds);
    }
}

void HTTPClient::setUserAgent(const std::string& user_agent) {
    if (curl_) {
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, user_agent.c_str());
    }
}

size_t HTTPClient::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t HTTPClient::HeaderCallback(void* contents, size_t size, size_t nmemb, 
                                 std::map<std::string, std::string>* userp) {
    size_t total_size = size * nmemb;
    std::string header(static_cast<char*>(contents), total_size);
    
    size_t colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        
        (*userp)[key] = value;
    }
    
    return total_size;
}

struct curl_slist* HTTPClient::buildHeaders(const std::map<std::string, std::string>& headers) {
    struct curl_slist* header_list = nullptr;
    
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }
    
    return header_list;
}

// LLMClient implementation
LLMClient::LLMClient(const LLMConfig& config) : config_(config) {
    http_client_ = std::make_unique<HTTPClient>();
}

LLMResponse LLMClient::chatCompletion(const std::vector<Message>& messages) {
    try {
        std::string payload = buildRequestPayload(messages);
        auto headers = buildHeaders();
        
        LOG_DEBUG("Sending request to: " + config_.api_endpoint);
        HTTPResponse http_response = http_client_->post(config_.api_endpoint, payload, headers);
        
        if (!http_response.success) {
            LLMResponse response;
            response.success = false;
            response.error_message = "HTTP request failed: " + http_response.error_message;
            response.status_code = http_response.status_code;
            return response;
        }
        
        return parseResponse(http_response);
    } catch (const std::exception& e) {
        LLMResponse response;
        response.success = false;
        response.error_message = "Exception in chatCompletion: " + std::string(e.what());
        return response;
    }
}

void LLMClient::streamChatCompletion(const std::vector<Message>& messages,
                                   std::function<void(const std::string&)> callback) {
    // For now, implement as non-streaming
    // TODO: Implement actual streaming support
    LLMResponse response = chatCompletion(messages);
    if (response.success) {
        callback(response.content);
    } else {
        callback("Error: " + response.error_message);
    }
}

void LLMClient::updateConfig(const LLMConfig& config) {
    config_ = config;
}

std::unique_ptr<LLMClient> LLMClient::createClient(const LLMConfig& config) {
    if (config.provider == "openai") {
        return std::make_unique<OpenAIClient>(config);
    } else if (config.provider == "anthropic") {
        return std::make_unique<AnthropicClient>(config);
    } else {
        return std::make_unique<CustomClient>(config);
    }
}

// OpenAIClient implementation
OpenAIClient::OpenAIClient(const LLMConfig& config) : LLMClient(config) {}

std::string OpenAIClient::buildRequestPayload(const std::vector<Message>& messages) {
    nlohmann::json payload;
    payload["model"] = config_.model_name;
    payload["temperature"] = config_.temperature;
    payload["max_tokens"] = config_.max_tokens;

    nlohmann::json json_messages = nlohmann::json::array();
    for (const auto& message : messages) {
        nlohmann::json json_msg;
        json_msg["role"] = message.role;
        json_msg["content"] = message.content;
        json_messages.push_back(json_msg);
    }
    payload["messages"] = json_messages;

    return payload.dump();
}

LLMResponse OpenAIClient::parseResponse(const HTTPResponse& http_response) {
    LLMResponse response;
    response.status_code = http_response.status_code;

    try {
        nlohmann::json json_response = nlohmann::json::parse(http_response.body);

        if (json_response.contains("error")) {
            response.success = false;
            response.error_message = json_response["error"]["message"];
            return response;
        }

        if (json_response.contains("choices") && !json_response["choices"].empty()) {
            response.success = true;
            response.content = json_response["choices"][0]["message"]["content"];

            // Extract metadata
            if (json_response.contains("usage")) {
                response.metadata["prompt_tokens"] = std::to_string(static_cast<int>(json_response["usage"]["prompt_tokens"]));
                response.metadata["completion_tokens"] = std::to_string(static_cast<int>(json_response["usage"]["completion_tokens"]));
                response.metadata["total_tokens"] = std::to_string(static_cast<int>(json_response["usage"]["total_tokens"]));
            }
        } else {
            response.success = false;
            response.error_message = "No choices in response";
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to parse JSON response: " + std::string(e.what());
    }

    return response;
}

std::map<std::string, std::string> OpenAIClient::buildHeaders() {
    std::map<std::string, std::string> headers = config_.headers;
    headers["Authorization"] = "Bearer " + config_.api_key;
    return headers;
}

// AnthropicClient implementation
AnthropicClient::AnthropicClient(const LLMConfig& config) : LLMClient(config) {}

std::string AnthropicClient::buildRequestPayload(const std::vector<Message>& messages) {
    nlohmann::json payload;
    payload["model"] = config_.model_name;
    payload["max_tokens"] = config_.max_tokens;

    // Anthropic uses different message format
    std::string system_message;
    nlohmann::json json_messages = nlohmann::json::array();

    for (const auto& message : messages) {
        if (message.role == "system") {
            system_message = message.content;
        } else {
            nlohmann::json json_msg;
            json_msg["role"] = message.role;
            json_msg["content"] = message.content;
            json_messages.push_back(json_msg);
        }
    }

    if (!system_message.empty()) {
        payload["system"] = system_message;
    }
    payload["messages"] = json_messages;

    return payload.dump();
}

LLMResponse AnthropicClient::parseResponse(const HTTPResponse& http_response) {
    LLMResponse response;
    response.status_code = http_response.status_code;

    try {
        nlohmann::json json_response = nlohmann::json::parse(http_response.body);

        if (json_response.contains("error")) {
            response.success = false;
            response.error_message = json_response["error"]["message"];
            return response;
        }

        if (json_response.contains("content") && !json_response["content"].empty()) {
            response.success = true;
            response.content = json_response["content"][0]["text"];

            // Extract metadata
            if (json_response.contains("usage")) {
                response.metadata["input_tokens"] = std::to_string(static_cast<int>(json_response["usage"]["input_tokens"]));
                response.metadata["output_tokens"] = std::to_string(static_cast<int>(json_response["usage"]["output_tokens"]));
            }
        } else {
            response.success = false;
            response.error_message = "No content in response";
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to parse JSON response: " + std::string(e.what());
    }

    return response;
}

std::map<std::string, std::string> AnthropicClient::buildHeaders() {
    std::map<std::string, std::string> headers = config_.headers;
    headers["x-api-key"] = config_.api_key;
    headers["anthropic-version"] = "2023-06-01";
    return headers;
}

// CustomClient implementation
CustomClient::CustomClient(const LLMConfig& config) : LLMClient(config) {}

std::string CustomClient::buildRequestPayload(const std::vector<Message>& messages) {
    // Generic implementation - similar to OpenAI format
    nlohmann::json payload;
    payload["model"] = config_.model_name;
    payload["temperature"] = config_.temperature;
    payload["max_tokens"] = config_.max_tokens;

    nlohmann::json json_messages = nlohmann::json::array();
    for (const auto& message : messages) {
        nlohmann::json json_msg;
        json_msg["role"] = message.role;
        json_msg["content"] = message.content;
        json_messages.push_back(json_msg);
    }
    payload["messages"] = json_messages;

    return payload.dump();
}

LLMResponse CustomClient::parseResponse(const HTTPResponse& http_response) {
    // Generic implementation - try to handle common response formats
    LLMResponse response;
    response.status_code = http_response.status_code;

    try {
        nlohmann::json json_response = nlohmann::json::parse(http_response.body);

        if (json_response.contains("error")) {
            response.success = false;
            response.error_message = json_response["error"].is_string() ?
                json_response["error"] : json_response["error"]["message"];
            return response;
        }

        // Try different response formats
        if (json_response.contains("choices") && !json_response["choices"].empty()) {
            // OpenAI-like format
            response.success = true;
            response.content = json_response["choices"][0]["message"]["content"];
        } else if (json_response.contains("content")) {
            // Anthropic-like format
            response.success = true;
            if (json_response["content"].is_array()) {
                response.content = json_response["content"][0]["text"];
            } else {
                response.content = json_response["content"];
            }
        } else if (json_response.contains("response")) {
            // Generic response field
            response.success = true;
            response.content = json_response["response"];
        } else {
            response.success = false;
            response.error_message = "Unknown response format";
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to parse JSON response: " + std::string(e.what());
    }

    return response;
}

std::map<std::string, std::string> CustomClient::buildHeaders() {
    std::map<std::string, std::string> headers = config_.headers;
    if (!config_.api_key.empty()) {
        headers["Authorization"] = "Bearer " + config_.api_key;
    }
    return headers;
}

} // namespace AITextAssistant
