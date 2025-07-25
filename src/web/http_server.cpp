#include "web/http_server.h"
#include "core/assistant.h"
#include "utils/logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <errno.h>
#include <cstring>

namespace AITextAssistant {

HttpServer::HttpServer(int port) : port_(port), running_(false), server_socket_(-1) {
    // Register default API routes (legacy format)
    addRoute("POST", "/api/chat", [this](const HttpRequest& req) { return handleApiChat(req); });
    addRoute("GET", "/api/conversations", [this](const HttpRequest& req) { return handleApiConversations(req); });
    addRoute("GET", "/api/conversations/messages", [this](const HttpRequest& req) { return handleApiConversationMessages(req); });
    addRoute("DELETE", "/api/conversations", [this](const HttpRequest& req) { return handleApiDeleteConversation(req); });
    addRoute("GET", "/api/status", [this](const HttpRequest& req) { return handleApiStatus(req); });

    // Add OpenAI-compatible API routes
    addRoute("POST", "/v1/chat/completions", [this](const HttpRequest& req) { return handleOpenAIChat(req); });
    addRoute("GET", "/v1/models", [this](const HttpRequest& req) { return handleOpenAIModels(req); });

    // CORS preflight requests
    addRoute("OPTIONS", "/api/chat", [](const HttpRequest&) {
        HttpResponse response;
        response.status_code = 200;
        return response;
    });

    addRoute("OPTIONS", "/api/conversations", [](const HttpRequest&) {
        HttpResponse response;
        response.status_code = 200;
        return response;
    });
    addRoute("OPTIONS", "/api/conversations", [](const HttpRequest&) {
        HttpResponse response;
        response.status_code = 200;
        return response;
    });
    addRoute("OPTIONS", "/v1/chat/completions", [](const HttpRequest&) {
        HttpResponse response;
        response.status_code = 200;
        return response;
    });
    addRoute("OPTIONS", "/v1/models", [](const HttpRequest&) {
        HttpResponse response;
        response.status_code = 200;
        return response;
    });
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_) {
        return false;
    }
    
    running_ = true;
    server_thread_ = std::thread(&HttpServer::serverLoop, this);
    
    LOG_INFO("HTTP server starting on port " + std::to_string(port_));
    return true;
}

void HttpServer::stop() {
    if (running_) {
        LOG_INFO("Stopping HTTP server...");
        running_ = false;

        // Close server socket to unblock accept()
        if (server_socket_ != -1) {
            LOG_INFO("Closing server socket...");
            close(server_socket_);
            server_socket_ = -1;
        }

        if (server_thread_.joinable()) {
            LOG_INFO("Waiting for server thread to join...");
            server_thread_.join();
            LOG_INFO("Server thread joined successfully");
        }
        LOG_INFO("HTTP server stopped");
    }
}

void HttpServer::addRoute(const std::string& method, const std::string& path, HttpHandler handler) {
    std::string key = method + " " + path;
    routes_[key] = handler;
}

void HttpServer::serverLoop() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        LOG_ERROR("Failed to create socket");
        running_ = false;
        return;
    }

    // Allow socket reuse
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set socket timeout to make accept() non-blocking
    struct timeval timeout;
    timeout.tv_sec = 1;  // 1 second timeout
    timeout.tv_usec = 0;
    setsockopt(server_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to bind socket to port " + std::to_string(port_));
        close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        return;
    }

    if (listen(server_socket_, 10) < 0) {
        LOG_ERROR("Failed to listen on socket");
        close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        return;
    }

    LOG_INFO("HTTP server listening on port " + std::to_string(port_));

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout occurred, check if we should continue
                continue;
            } else if (running_) {
                LOG_ERROR("Failed to accept client connection: " + std::string(strerror(errno)));
            } else {
                // Server is shutting down, break the loop
                LOG_INFO("Server shutting down, exiting accept loop");
                break;
            }
            continue;
        }

        // Handle client in a separate thread
        std::thread client_thread(&HttpServer::handleClient, this, client_socket);
        client_thread.detach();
    }

    if (server_socket_ != -1) {
        close(server_socket_);
        server_socket_ = -1;
    }
}

void HttpServer::handleClient(int client_socket) {
    std::string request_data;
    char buffer[4096];
    ssize_t bytes_read;
    
    // Keep receiving data until we have a complete HTTP request
    while ((bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        request_data += std::string(buffer);
        
        // Check if we have received the complete HTTP headers (ending with \r\n\r\n)
        size_t header_end = request_data.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            // Parse Content-Length to determine if we need to read more body data
            size_t content_length = 0;
            size_t content_length_pos = request_data.find("Content-Length:");
            if (content_length_pos != std::string::npos) {
                size_t value_start = request_data.find(":", content_length_pos) + 1;
                size_t value_end = request_data.find("\r\n", value_start);
                std::string length_str = request_data.substr(value_start, value_end - value_start);
                // Trim whitespace
                length_str.erase(0, length_str.find_first_not_of(" \t"));
                length_str.erase(length_str.find_last_not_of(" \t") + 1);
                content_length = std::stoul(length_str);
            }
            
            size_t body_start = header_end + 4;
            size_t current_body_length = request_data.length() - body_start;
            
            // If we have all the body data, break
            if (current_body_length >= content_length) {
                break;
            }
        }
    }
    
    if (bytes_read < 0 || request_data.empty()) {
        close(client_socket);
        return;
    }
    
    HttpRequest request = parseRequest(request_data);
    HttpResponse response = handleRequest(request);
    std::string response_str = buildResponse(response);
    
    send(client_socket, response_str.c_str(), response_str.length(), 0);
    close(client_socket);
}

HttpRequest HttpServer::parseRequest(const std::string& request_data) {
    HttpRequest request;
    std::istringstream stream(request_data);
    std::string line;
    
    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        std::string path_with_query;
        line_stream >> request.method >> path_with_query;
        
        // Parse path and query parameters
        size_t query_pos = path_with_query.find('?');
        if (query_pos != std::string::npos) {
            request.path = path_with_query.substr(0, query_pos);
            std::string query = path_with_query.substr(query_pos + 1);
            request.query_params = parseQueryString(query);
        } else {
            request.path = path_with_query;
        }
    }
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            request.headers[key] = value;
        }
    }
    
    // Parse body
    std::string body;
    while (std::getline(stream, line)) {
        body += line + "\n";
    }
    if (!body.empty()) {
        body.pop_back(); // Remove last newline
    }
    request.body = body;
    
    return request;
}

std::string HttpServer::buildResponse(const HttpResponse& response) {
    std::ostringstream stream;
    stream << "HTTP/1.1 " << response.status_code << " ";
    
    // Status text
    switch (response.status_code) {
        case 200: stream << "OK"; break;
        case 404: stream << "Not Found"; break;
        case 500: stream << "Internal Server Error"; break;
        default: stream << "Unknown"; break;
    }
    stream << "\r\n";
    
    // Headers
    for (const auto& header : response.headers) {
        stream << header.first << ": " << header.second << "\r\n";
    }
    stream << "Content-Length: " << response.body.length() << "\r\n";
    stream << "\r\n";
    
    // Body
    stream << response.body;
    
    return stream.str();
}

HttpResponse HttpServer::handleRequest(const HttpRequest& request) {
    // Try to find exact route match
    std::string route_key = request.method + " " + request.path;
    auto route_it = routes_.find(route_key);

    HttpResponse response;

    if (route_it != routes_.end()) {
        try {
            response = route_it->second(request);
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling route " + route_key + ": " + e.what());
            response.status_code = 500;
            response.body = "Internal Server Error";
        }
    } else {
        // Handle route not found
        response = handleStaticFile(request);
    }

    // Add CORS headers to all responses
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    response.headers["Access-Control-Max-Age"] = "86400";

    return response;
}

std::map<std::string, std::string> HttpServer::parseQueryString(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream stream(query);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, eq_pos));
            std::string value = urlDecode(pair.substr(eq_pos + 1));
            params[key] = value;
        }
    }
    
    return params;
}

std::string HttpServer::urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int hex_value;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            if (hex_stream >> std::hex >> hex_value) {
                result += static_cast<char>(hex_value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

HttpResponse HttpServer::handleApiChat(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    if (!assistant_) {
        response.status_code = 500;
        response.body = R"({"error": "Assistant not available"})";
        return response;
    }

    try {
        nlohmann::json request_json = nlohmann::json::parse(request.body);
        std::string message = request_json["message"];

        // Handle conversation switching
        std::string conversation_id;
        if (request_json.contains("conversation_id") && !request_json["conversation_id"].is_null()) {
            conversation_id = request_json["conversation_id"];

            // Load the specified conversation if it's different from current
            if (!conversation_id.empty()) {
                if (!assistant_->loadConversation(conversation_id)) {
                    // If conversation doesn't exist, create a new one
                    conversation_id = assistant_->startNewConversation();
                    if (conversation_id.empty()) {
                        response.status_code = 500;
                        response.body = R"({"error": "Failed to create new conversation"})";
                        return response;
                    }
                }
            }
        } else {
            // Create new conversation if none specified
            conversation_id = assistant_->startNewConversation();
            if (conversation_id.empty()) {
                response.status_code = 500;
                response.body = R"({"error": "Failed to create new conversation"})";
                return response;
            }
        }

        // Check message length (max 8000 characters for user input)
        const size_t MAX_USER_MESSAGE_LENGTH = 8000;
        if (message.length() > MAX_USER_MESSAGE_LENGTH) {
            response.status_code = 400;
            nlohmann::json error_json;
            error_json["error"] = "Message too long. Maximum length is " + std::to_string(MAX_USER_MESSAGE_LENGTH) + " characters.";
            error_json["current_length"] = message.length();
            response.body = error_json.dump();
            return response;
        }

        // Process the message through the assistant
        std::string assistant_response = assistant_->processTextInput(message);

        nlohmann::json response_json;
        response_json["status"] = "success";
        response_json["conversation_id"] = conversation_id;
        response_json["response"] = assistant_response;
        response_json["is_split"] = false;

        response.body = response_json.dump();

    } catch (const std::exception& e) {
        response.status_code = 400;
        nlohmann::json error_json;
        error_json["error"] = "Invalid request: " + std::string(e.what());
        response.body = error_json.dump();
    }

    return response;
}

HttpResponse HttpServer::handleApiConversations(const HttpRequest& /* request */) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    if (!assistant_) {
        response.status_code = 500;
        response.body = R"({"error": "Assistant not available"})";
        return response;
    }

    try {
        auto conversations = assistant_->getRecentConversations(10);
        nlohmann::json response_json;
        response_json["conversations"] = nlohmann::json::array();

        for (const auto& conv : conversations) {
            nlohmann::json conv_json;
            conv_json["id"] = conv.id;
            conv_json["title"] = conv.title;
            conv_json["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                conv.created_at.time_since_epoch()).count();
            response_json["conversations"].push_back(conv_json);
        }

        response.body = response_json.dump();

    } catch (const std::exception& e) {
        response.status_code = 500;
        nlohmann::json error_json;
        error_json["error"] = "Failed to get conversations: " + std::string(e.what());
        response.body = error_json.dump();
    }

    return response;
}

HttpResponse HttpServer::handleApiDeleteConversation(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    if (!assistant_) {
        response.status_code = 500;
        response.body = R"({"error": "Assistant not available"})";
        return response;
    }

    try {
        nlohmann::json request_json = nlohmann::json::parse(request.body);
        std::string conversation_id = request_json["conversation_id"];

        if (conversation_id.empty()) {
            response.status_code = 400;
            response.body = R"({"error": "conversation_id is required"})";
            return response;
        }

        // Delete the conversation
        bool success = assistant_->deleteConversation(conversation_id);

        nlohmann::json response_json;
        if (success) {
            response_json["success"] = true;
            response_json["message"] = "Conversation deleted successfully";
        } else {
            response.status_code = 404;
            response_json["error"] = "Conversation not found or could not be deleted";
        }

        response.body = response_json.dump();

    } catch (const std::exception& e) {
        response.status_code = 400;
        nlohmann::json error_json;
        error_json["error"] = "Invalid request: " + std::string(e.what());
        response.body = error_json.dump();
    }

    return response;
}

HttpResponse HttpServer::handleApiStatus(const HttpRequest& /* request */) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    nlohmann::json status_json;
    status_json["status"] = "running";
    status_json["version"] = "1.0.0";
    status_json["assistant_available"] = (assistant_ != nullptr);

    if (assistant_) {
        status_json["total_conversations"] = assistant_->getTotalConversations();
        status_json["total_messages"] = assistant_->getTotalMessages();
    }

    response.body = status_json.dump();
    return response;
}

HttpResponse HttpServer::handleStaticFile(const HttpRequest& request) {
    HttpResponse response;

    std::string file_path = static_directory_ + request.path;

    // Default to index.html for root path
    if (request.path == "/") {
        file_path += "/index.html";
    }

    // Security check - prevent directory traversal
    if (file_path.find("..") != std::string::npos) {
        response.status_code = 403;
        response.body = "Forbidden";
        return response;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        response.status_code = 404;
        response.body = "File not found";
        return response;
    }

    // Read file content
    std::ostringstream content_stream;
    content_stream << file.rdbuf();
    response.body = content_stream.str();

    // Set appropriate content type
    response.headers["Content-Type"] = getMimeType(file_path);

    return response;
}

std::string HttpServer::getMimeType(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "text/plain";
    }

    std::string extension = filename.substr(dot_pos + 1);

    if (extension == "html" || extension == "htm") return "text/html";
    if (extension == "css") return "text/css";
    if (extension == "js") return "application/javascript";
    if (extension == "json") return "application/json";
    if (extension == "png") return "image/png";
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "gif") return "image/gif";
    if (extension == "svg") return "image/svg+xml";
    if (extension == "ico") return "image/x-icon";

    return "text/plain";
}

HttpResponse HttpServer::handleOpenAIChat(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    if (!assistant_) {
        response.status_code = 500;
        response.body = R"({"error": {"message": "Assistant not available", "type": "internal_error"}})";
        return response;
    }

    try {
        nlohmann::json request_json = nlohmann::json::parse(request.body);

        // Extract messages from OpenAI format
        if (!request_json.contains("messages") || !request_json["messages"].is_array()) {
            response.status_code = 400;
            response.body = R"({"error": {"message": "Missing or invalid messages field", "type": "invalid_request_error"}})";
            return response;
        }

        auto messages = request_json["messages"];
        std::string user_message;

        // Get the last user message
        for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
            if ((*it)["role"] == "user") {
                user_message = (*it)["content"];
                break;
            }
        }

        if (user_message.empty()) {
            response.status_code = 400;
            response.body = R"({"error": {"message": "No user message found", "type": "invalid_request_error"}})";
            return response;
        }

        // Check if streaming is requested
        bool stream = request_json.value("stream", false);

        // Process the message through the assistant
        std::string assistant_response = assistant_->processTextInput(user_message);

        if (stream) {
            // For streaming, we'll send a simple non-streaming response for now
            // In a full implementation, you'd want to implement Server-Sent Events
            nlohmann::json response_json;
            response_json["id"] = "chatcmpl-" + std::to_string(std::time(nullptr));
            response_json["object"] = "chat.completion";
            response_json["created"] = std::time(nullptr);
            response_json["model"] = request_json.value("model", "gpt-3.5-turbo");
            response_json["choices"] = nlohmann::json::array();

            nlohmann::json choice;
            choice["index"] = 0;
            choice["message"]["role"] = "assistant";
            choice["message"]["content"] = assistant_response;
            choice["finish_reason"] = "stop";

            response_json["choices"].push_back(choice);
            response_json["usage"]["prompt_tokens"] = user_message.length() / 4; // Rough estimate
            response_json["usage"]["completion_tokens"] = assistant_response.length() / 4;
            response_json["usage"]["total_tokens"] = response_json["usage"]["prompt_tokens"].get<int>() +
                                                   response_json["usage"]["completion_tokens"].get<int>();

            response.body = response_json.dump();
        } else {
            // Non-streaming response
            nlohmann::json response_json;
            response_json["id"] = "chatcmpl-" + std::to_string(std::time(nullptr));
            response_json["object"] = "chat.completion";
            response_json["created"] = std::time(nullptr);
            response_json["model"] = request_json.value("model", "gpt-3.5-turbo");
            response_json["choices"] = nlohmann::json::array();

            nlohmann::json choice;
            choice["index"] = 0;
            choice["message"]["role"] = "assistant";
            choice["message"]["content"] = assistant_response;
            choice["finish_reason"] = "stop";

            response_json["choices"].push_back(choice);
            response_json["usage"]["prompt_tokens"] = user_message.length() / 4; // Rough estimate
            response_json["usage"]["completion_tokens"] = assistant_response.length() / 4;
            response_json["usage"]["total_tokens"] = response_json["usage"]["prompt_tokens"].get<int>() +
                                                   response_json["usage"]["completion_tokens"].get<int>();

            response.body = response_json.dump();
        }

    } catch (const std::exception& e) {
        response.status_code = 400;
        nlohmann::json error_json;
        error_json["error"]["message"] = "Invalid request: " + std::string(e.what());
        error_json["error"]["type"] = "invalid_request_error";
        response.body = error_json.dump();
    }

    return response;
}

HttpResponse HttpServer::handleOpenAIModels(const HttpRequest& /* request */) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    nlohmann::json models_json;
    models_json["object"] = "list";
    models_json["data"] = nlohmann::json::array();

    // Add a default model
    nlohmann::json model;
    model["id"] = "gpt-3.5-turbo";
    model["object"] = "model";
    model["created"] = std::time(nullptr);
    model["owned_by"] = "ai-assistant";

    models_json["data"].push_back(model);

    response.body = models_json.dump();
    return response;
}

HttpResponse HttpServer::handleApiConversationMessages(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    if (!assistant_) {
        response.status_code = 500;
        response.body = R"({"error": "Assistant not available"})";
        return response;
    }

    try {
        // Get conversation_id from query parameters
        std::string conversation_id;
        if (request.query_params.find("conversation_id") != request.query_params.end()) {
            conversation_id = request.query_params.at("conversation_id");
        }

        if (conversation_id.empty()) {
            response.status_code = 400;
            response.body = R"({"error": "conversation_id parameter is required"})";
            return response;
        }

        // Load conversation from database
        if (!assistant_->loadConversation(conversation_id)) {
            response.status_code = 404;
            response.body = R"({"error": "Conversation not found"})";
            return response;
        }

        // Get conversation history from the assistant
        auto conversation_history = assistant_->getCurrentConversationHistory();
        nlohmann::json response_json;
        response_json["conversation_id"] = conversation_id;
        response_json["messages"] = nlohmann::json::array();

        // Convert messages to JSON
        for (const auto& msg : conversation_history) {
            nlohmann::json msg_json;
            msg_json["role"] = msg.role;
            msg_json["content"] = msg.content;
            msg_json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                msg.timestamp.time_since_epoch()).count();
            response_json["messages"].push_back(msg_json);
        }

        response.body = response_json.dump();

    } catch (const std::exception& e) {
        response.status_code = 500;
        nlohmann::json error_json;
        error_json["error"] = "Failed to get conversation messages: " + std::string(e.what());
        response.body = error_json.dump();
    }

    return response;
}

} // namespace AITextAssistant
