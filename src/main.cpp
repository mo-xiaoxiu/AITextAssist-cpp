#include "core/assistant.h"
#include "web/http_server.h"
#include "utils/logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

using namespace AITextAssistant;

// Global assistant instance for signal handling
std::unique_ptr<TextAssistant> g_assistant;
std::unique_ptr<HttpServer> g_http_server;
std::atomic<bool> g_running{true};

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    static std::atomic<bool> shutdown_initiated{false};

    if (shutdown_initiated.exchange(true)) {
        // If shutdown already initiated, force exit
        std::cout << "\nForce shutdown..." << std::endl;
        std::exit(1);
    }

    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    g_running = false;

    // Stop HTTP server if running
    if (g_http_server) {
        g_http_server->stop();
    }
}

// Print usage information
void printUsage(const char* program_name) {
    std::cout << "AI Text Assistant v1.0.0\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -c, --config FILE       Use custom configuration file\n";
    std::cout << "  -l, --log-level LEVEL   Set log level (DEBUG, INFO, WARN, ERROR)\n";
    std::cout << "  --log-file FILE         Write logs to file\n";
    std::cout << "  --test                  Test connections and exit\n";
    std::cout << "  --interactive           Interactive configuration mode\n";
    std::cout << "  --web                   Start web interface mode\n";
    std::cout << "  --port PORT             Web server port (default: 8080)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << "                    # Start with default settings (text mode)\n";
    std::cout << "  " << program_name << " --web              # Start web interface on port 8080\n";
    std::cout << "  " << program_name << " --web --port 3000  # Start web interface on port 3000\n";
    std::cout << "  " << program_name << " -c my_config.json  # Use custom config\n";
    std::cout << "  " << program_name << " --test             # Test configuration\n";
}

// Parse command line arguments
struct CommandLineArgs {
    std::string config_file = "../config/default_config.json";
    std::string log_level = "INFO";
    std::string log_file = "";
    bool show_help = false;
    bool test_mode = false;
    bool interactive_config = false;
    bool web_mode = false;
    int web_port = 8080;
};

CommandLineArgs parseArgs(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.show_help = true;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                args.config_file = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a filename\n";
                args.show_help = true;
            }
        } else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                args.log_level = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a log level\n";
                args.show_help = true;
            }
        } else if (arg == "--log-file") {
            if (i + 1 < argc) {
                args.log_file = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a filename\n";
                args.show_help = true;
            }
        } else if (arg == "--test") {
            args.test_mode = true;
        } else if (arg == "--interactive") {
            args.interactive_config = true;
        } else if (arg == "--web") {
            args.web_mode = true;
        } else if (arg == "--port") {
            if (i + 1 < argc) {
                args.web_port = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: " << arg << " requires a port number\n";
                args.show_help = true;
            }
        } else {
            std::cerr << "Error: Unknown option " << arg << "\n";
            args.show_help = true;
        }
    }
    
    return args;
}

// Interactive configuration setup
void interactiveConfig(TextAssistant& assistant) {
    std::cout << "\n=== Interactive Configuration ===\n";
    
    std::string input;
    
    // LLM Configuration
    std::cout << "\nLLM Configuration:\n";
    std::cout << "Available providers: openai, anthropic, custom\n";
    std::cout << "Enter LLM provider (or press Enter for default): ";
    std::getline(std::cin, input);
    
    if (!input.empty()) {
        LLMConfig llm_config;
        llm_config.provider = input;
        
        std::cout << "Enter API endpoint: ";
        std::getline(std::cin, llm_config.api_endpoint);
        
        std::cout << "Enter API key: ";
        std::getline(std::cin, llm_config.api_key);
        
        std::cout << "Enter model name: ";
        std::getline(std::cin, llm_config.model_name);
        
        assistant.setLLMProvider(llm_config);
    }
    
    // Prompt Template
    std::cout << "\nPrompt Templates:\n";
    std::cout << "Available templates: default, casual, professional, technical, creative\n";
    std::cout << "Enter prompt template (or press Enter for default): ";
    std::getline(std::cin, input);
    
    if (!input.empty()) {
        assistant.loadPromptTemplate(input);
    }
    
    std::cout << "\nConfiguration completed!\n";
}

// Web-based interaction mode
void webMode(TextAssistant& assistant, int port) {
    std::cout << "\n=== Web server Mode ===\n";
    std::cout << "Starting HTTP server on port " << port << "...\n";

    // Create HTTP server
    g_http_server = std::make_unique<HttpServer>(port);
    g_http_server->setAssistant(std::shared_ptr<TextAssistant>(&assistant, [](TextAssistant*){}));
    g_http_server->setStaticDirectory("web");

    if (!g_http_server->start()) {
        std::cerr << "Failed to start HTTP server on port " << port << std::endl;
        return;
    }

    std::cout << "Web server is now available at: http://localhost:" << port << std::endl;
    std::cout << "Press Ctrl+C to stop the server\n\n";

    // Keep the server running
    while (g_running && g_http_server->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down web server...\n";
    g_http_server->stop();
    g_http_server.reset();
}

// Text-based interaction loop
void textMode(TextAssistant& assistant) {
    std::cout << "\n=== Text Mode ===\n";
    std::cout << "Type 'quit' or 'exit' to stop, 'help' for commands\n\n";
    
    std::string input;
    while (g_running) {
        std::cout << "You: ";
        if (!std::getline(std::cin, input)) {
            break; // EOF
        }
        
        if (input.empty()) continue;
        
        if (input == "quit" || input == "exit") {
            break;
        } else if (input == "help") {
            std::cout << "\nAvailable commands:\n";
            std::cout << "  help              - Show this help\n";
            std::cout << "  quit              - Exit the program\n";
            std::cout << "  clear             - Clear conversation history\n";
            std::cout << "  save              - Save current conversation\n";
            std::cout << "  stats             - Show statistics\n";
            std::cout << "  config            - Show current configuration\n";
            std::cout << "  list              - List recent conversations\n";
            std::cout << "  delete <id>       - Delete a conversation by ID\n\n";
            continue;
        } else if (input == "clear") {
            assistant.clearConversationHistory();
            std::cout << "Conversation history cleared.\n\n";
            continue;
        } else if (input == "save") {
            if (assistant.saveCurrentConversation()) {
                std::cout << "Conversation saved.\n\n";
            } else {
                std::cout << "Failed to save conversation.\n\n";
            }
            continue;
        } else if (input == "stats") {
            std::cout << "Total conversations: " << assistant.getTotalConversations() << "\n";
            std::cout << "Total messages: " << assistant.getTotalMessages() << "\n\n";
            continue;
        } else if (input == "config") {
            std::cout << assistant.getSystemInfo() << "\n\n";
            continue;
        } else if (input == "list") {
            auto conversations = assistant.getRecentConversations(10);
            std::cout << "\nRecent conversations:\n";
            if (conversations.empty()) {
                std::cout << "No conversations found.\n\n";
            } else {
                for (size_t i = 0; i < conversations.size(); ++i) {
                    const auto& conv = conversations[i];
                    std::cout << (i + 1) << ". " << (conv.title.empty() ? "Untitled" : conv.title) << "\n";
                    std::cout << "    ID: " << conv.id << "\n";

                    // Show creation time
                    auto time_t = std::chrono::system_clock::to_time_t(conv.created_at);
                    std::cout << "    Created: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";
                }
            }
            continue;
        } else if (input.substr(0, 7) == "delete ") {
            std::string conv_input = input.substr(7);
            if (conv_input.empty()) {
                std::cout << "Usage: delete <conversation_id_or_number>\n";
                std::cout << "You can use either the full conversation ID or the number from 'list' command.\n\n";
                continue;
            }

            std::string conv_id = conv_input;

            // Check if input is a number (referring to list index)
            if (std::all_of(conv_input.begin(), conv_input.end(), ::isdigit)) {
                int index = std::stoi(conv_input) - 1;
                auto conversations = assistant.getRecentConversations(10);

                if (index >= 0 && index < static_cast<int>(conversations.size())) {
                    conv_id = conversations[index].id;
                    std::cout << "Selected conversation: " << (conversations[index].title.empty() ? "Untitled" : conversations[index].title) << "\n";
                } else {
                    std::cout << "Invalid conversation number. Use 'list' to see available conversations.\n\n";
                    continue;
                }
            }

            std::cout << "Are you sure you want to delete this conversation? (y/N): ";
            std::string confirm;
            std::getline(std::cin, confirm);

            if (confirm == "y" || confirm == "Y" || confirm == "yes") {
                if (assistant.deleteConversation(conv_id)) {
                    std::cout << "Conversation deleted successfully.\n\n";
                } else {
                    std::cout << "Failed to delete conversation. Check the ID and try again.\n\n";
                }
            } else {
                std::cout << "Deletion cancelled.\n\n";
            }
            continue;
        }
        
        // Process user input
        std::string response = assistant.processTextInput(input);

        // Display response with proper UTF-8 formatting
        std::cout << "Assistant: ";
        std::cout.flush();

        // Simple approach: split by sentences and punctuation to avoid breaking UTF-8 characters
        const size_t max_line_length = 100; // Increased for better display
        size_t pos = 0;

        while (pos < response.length()) {
            size_t end_pos = pos + max_line_length;

            // If we're at the end, just output the rest
            if (end_pos >= response.length()) {
                std::cout << response.substr(pos) << std::endl;
                break;
            }

            // Look for good break points (punctuation, spaces)
            size_t break_pos = end_pos;

            // First, try to find sentence endings
            for (size_t i = end_pos; i > pos && i > end_pos - 30; i--) {
                // Check for ASCII punctuation
                char c = response[i];
                if (c == '.' || c == '!' || c == '?') {
                    break_pos = i + 1;
                    break;
                }

                // Check for Chinese punctuation (UTF-8 sequences)
                if (i >= 2) {
                    // Chinese period: 。 (E3 80 82)
                    if ((unsigned char)response[i-2] == 0xE3 &&
                        (unsigned char)response[i-1] == 0x80 &&
                        (unsigned char)response[i] == 0x82) {
                        break_pos = i + 1;
                        break;
                    }
                    // Chinese exclamation: ！ (EF BC 81)
                    if ((unsigned char)response[i-2] == 0xEF &&
                        (unsigned char)response[i-1] == 0xBC &&
                        (unsigned char)response[i] == 0x81) {
                        break_pos = i + 1;
                        break;
                    }
                    // Chinese question mark: ？ (EF BC 9F)
                    if ((unsigned char)response[i-2] == 0xEF &&
                        (unsigned char)response[i-1] == 0xBC &&
                        (unsigned char)response[i] == 0x9F) {
                        break_pos = i + 1;
                        break;
                    }
                }
            }

            // If no sentence ending found, look for spaces or commas
            if (break_pos == end_pos) {
                for (size_t i = end_pos; i > pos && i > end_pos - 20; i--) {
                    char c = response[i];
                    if (c == ' ' || c == ',') {
                        break_pos = i + 1;
                        break;
                    }

                    // Check for Chinese comma (UTF-8 sequences)
                    if (i >= 2) {
                        // Chinese comma: ，(EF BC 8C)
                        if ((unsigned char)response[i-2] == 0xEF &&
                            (unsigned char)response[i-1] == 0xBC &&
                            (unsigned char)response[i] == 0x8C) {
                            break_pos = i + 1;
                            break;
                        }
                        // Chinese pause mark: 、(E3 80 81)
                        if ((unsigned char)response[i-2] == 0xE3 &&
                            (unsigned char)response[i-1] == 0x80 &&
                            (unsigned char)response[i] == 0x81) {
                            break_pos = i + 1;
                            break;
                        }
                    }
                }
            }

            // If still no good break point, find a safe UTF-8 boundary
            if (break_pos == end_pos) {
                // Move back to find a UTF-8 character boundary
                while (break_pos > pos && (response[break_pos] & 0xC0) == 0x80) {
                    break_pos--;
                }
            }

            // Output this line
            std::string line = response.substr(pos, break_pos - pos);
            // Trim trailing spaces
            while (!line.empty() && line.back() == ' ') {
                line.pop_back();
            }
            std::cout << line << std::endl;

            // Skip any leading spaces on the next line
            pos = break_pos;
            while (pos < response.length() && response[pos] == ' ') {
                pos++;
            }
        }

        std::cout << std::endl;
        std::cout.flush();
    }
}



int main(int argc, char* argv[]) {
    // Parse command line arguments
    CommandLineArgs args = parseArgs(argc, argv);
    
    if (args.show_help) {
        printUsage(argv[0]);
        return 0;
    }
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Initialize logger
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(Logger::stringToLogLevel(args.log_level));
    
    if (!args.log_file.empty()) {
        logger.setLogFile(args.log_file);
    }
    
    LOG_INFO("Starting AI Text Assistant v1.0.0");
    
    try {
        // Create and initialize assistant
        g_assistant = std::make_unique<TextAssistant>(args.config_file);
        
        if (!g_assistant->initialize()) {
            LOG_ERROR("Failed to initialize assistant");
            return 1;
        }
        
        // Test mode
        if (args.test_mode) {
            std::cout << "Testing connections...\n";
            if (g_assistant->testConnections()) {
                std::cout << "All connections successful!\n";
                return 0;
            } else {
                std::cout << "Some connections failed. Check configuration.\n";
                return 1;
            }
        }
        
        // Interactive configuration
        if (args.interactive_config) {
            interactiveConfig(*g_assistant);
        }
        
        // Start new conversation
        std::string conversation_id = g_assistant->startNewConversation();
        LOG_INFO("Started new conversation: " + conversation_id);

        // Main interaction loop
        if (args.web_mode) {
            webMode(*g_assistant, args.web_port);
        } else {
            textMode(*g_assistant);
        }
        
        // Save conversation before exit
        g_assistant->saveCurrentConversation();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in main: " + std::string(e.what()));
        return 1;
    }
    
    LOG_INFO("AI Text Assistant shutting down");
    return 0;
}
