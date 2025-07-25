#pragma once

#include <string>
#include <functional>
#include <map>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>

namespace AITextAssistant {

// Forward declaration
class TextAssistant;

// HTTP request structure
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
};

// HTTP response structure
struct HttpResponse {
    int status_code = 200;
    std::string body;
    std::map<std::string, std::string> headers;
    
    HttpResponse() {
        headers["Content-Type"] = "text/html; charset=utf-8";
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type";
    }
};

// HTTP handler function type
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

// Simple HTTP server class
class HttpServer {
public:
    explicit HttpServer(int port = 8080);
    ~HttpServer();
    
    // Server control
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Route registration
    void addRoute(const std::string& method, const std::string& path, HttpHandler handler);
    void setAssistant(std::shared_ptr<TextAssistant> assistant) { assistant_ = assistant; }
    
    // Static file serving
    void setStaticDirectory(const std::string& directory) { static_directory_ = directory; }
    
private:
    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::map<std::string, HttpHandler> routes_;
    std::shared_ptr<TextAssistant> assistant_;
    std::string static_directory_;
    int server_socket_;
    
    // Server implementation
    void serverLoop();
    void handleClient(int client_socket);
    HttpRequest parseRequest(const std::string& request_data);
    std::string buildResponse(const HttpResponse& response);
    HttpResponse handleRequest(const HttpRequest& request);
    
    // Built-in handlers
    HttpResponse handleStaticFile(const HttpRequest& request);
    HttpResponse handleApiChat(const HttpRequest& request);
    HttpResponse handleApiConversations(const HttpRequest& request);
    HttpResponse handleApiConversationMessages(const HttpRequest& request);
    HttpResponse handleApiDeleteConversation(const HttpRequest& request);
    HttpResponse handleApiStatus(const HttpRequest& request);

    // OpenAI-compatible handlers
    HttpResponse handleOpenAIChat(const HttpRequest& request);
    HttpResponse handleOpenAIModels(const HttpRequest& request);
    
    // Utility functions
    std::string getMimeType(const std::string& filename);
    std::string urlDecode(const std::string& str);
    std::map<std::string, std::string> parseQueryString(const std::string& query);
};

} // namespace AITextAssistant
