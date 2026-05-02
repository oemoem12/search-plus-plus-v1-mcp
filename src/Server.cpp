#include <mcpp/Server.h>
#include <thread>
#include <iostream>
#include <chrono>

namespace mcpp {

McpServer::McpServer(std::string name, std::string version,
                     std::optional<std::string> instructions)
    : server_name_(std::move(name)),
      server_version_(std::move(version)),
      instructions_(std::move(instructions))
{
    setupHandlers();
    updateCapabilities();
}

void McpServer::setupHandlers() {
    // Initialize request (must be first message from client)
    dispatcher_.registerMethod("initialize", [this](const json& params) {
        return handleInitialize(params);
    });

    // Initialized notification
    dispatcher_.registerNotification("notifications/initialized", [this](const json& params) {
        handleInitialized(params);
    });

    // Ping
    dispatcher_.registerMethod("ping", [this](const json& params) {
        return handlePing(params);
    });

    // Tools
    dispatcher_.registerMethod("tools/list", [this](const json& params) {
        return handleToolsList(params);
    });
    dispatcher_.registerMethod("tools/call", [this](const json& params) {
        return handleToolsCall(params);
    });

    // Resources
    dispatcher_.registerMethod("resources/list", [this](const json& params) {
        return handleResourcesList(params);
    });
    dispatcher_.registerMethod("resources/read", [this](const json& params) {
        return handleResourcesRead(params);
    });

    // Prompts
    dispatcher_.registerMethod("prompts/list", [this](const json& params) {
        return handlePromptsList(params);
    });
    dispatcher_.registerMethod("prompts/get", [this](const json& params) {
        return handlePromptsGet(params);
    });

    // Logging
    dispatcher_.registerNotification("logging/setLevel", [this](const json& params) {
        handleLoggingSetLevel(params);
    });
}

void McpServer::updateCapabilities() {
    // Tools capability
    if (tools_.toolCount() > 0) {
        ServerCapabilities::ToolsCapability tc;
        tc.listChanged = true;
        capabilities_.tools = std::move(tc);
    } else {
        capabilities_.tools = std::nullopt;
    }

    // Resources capability
    if (resources_.resourceCount() > 0) {
        ServerCapabilities::ResourcesCapability rc;
        rc.subscribe = false;
        rc.listChanged = true;
        capabilities_.resources = std::move(rc);
    } else {
        capabilities_.resources = std::nullopt;
    }

    // Prompts capability
    if (prompts_.promptCount() > 0) {
        ServerCapabilities::PromptsCapability pc;
        pc.listChanged = true;
        capabilities_.prompts = std::move(pc);
    } else {
        capabilities_.prompts = std::nullopt;
    }
}

json McpServer::handleInitialize(const json& params) {
    // Parse client info and protocol version
    std::string client_protocol_version = "2024-11-05"; // default
    try {
        if (params.contains("protocolVersion") && params["protocolVersion"].is_string()) {
            client_protocol_version = params["protocolVersion"].get<std::string>();
        }
    } catch (...) {
        // Ignore parse errors
    }

    InitializeResult result;
    // Return the client's protocol version (MCP spec: server MUST respond with same version if supported)
    result.protocolVersion = client_protocol_version;
    result.capabilities = capabilities_;
    result.serverInfo.name = server_name_;
    result.serverInfo.version = server_version_;
    result.instructions = instructions_;

    return result.to_json();
}

void McpServer::handleInitialized([[maybe_unused]] const json& params) {
    initialized_.store(true, std::memory_order_relaxed);
}

json McpServer::handleToolsList([[maybe_unused]] const json& params) {
    return json{
        {"tools", tools_.listTools()}
    };
}

json McpServer::handleToolsCall(const json& params) {
    if (!params.contains("name")) {
        throw std::runtime_error("Missing required field: name");
    }

    const std::string tool_name = params.at("name").get<std::string>();
    const json tool_params = params.contains("arguments") ? params.at("arguments") : json::object();

    ToolResult result = tools_.callTool(tool_name, tool_params);
    return result.to_json();
}

json McpServer::handleResourcesList([[maybe_unused]] const json& params) {
    return json{
        {"resources", resources_.listResources()}
    };
}

json McpServer::handleResourcesRead(const json& params) {
    if (!params.contains("uri")) {
        throw std::runtime_error("Missing required field: uri");
    }

    const std::string uri = params.at("uri").get<std::string>();
    json contents = resources_.readResource(uri);

    return json{
        {"contents", std::move(contents)}
    };
}

json McpServer::handlePromptsList([[maybe_unused]] const json& params) {
    return json{
        {"prompts", prompts_.listPrompts()}
    };
}

json McpServer::handlePromptsGet(const json& params) {
    if (!params.contains("name")) {
        throw std::runtime_error("Missing required field: name");
    }

    const std::string prompt_name = params.at("name").get<std::string>();

    std::unordered_map<std::string, std::string> args;
    if (params.contains("arguments") && params.at("arguments").is_object()) {
        const auto& args_obj = params.at("arguments");
        for (auto it = args_obj.begin(); it != args_obj.end(); ++it) {
            args[it.key()] = it.value().get<std::string>();
        }
    }

    PromptResult result = prompts_.getPrompt(prompt_name, args);
    return result.to_json();
}

void McpServer::handleLoggingSetLevel(const json& params) {
    if (params.contains("level")) {
        try {
            min_log_level_ = params.at("level").get<LogLevel>();
        } catch (...) {
            // Ignore invalid level
        }
    }
}

json McpServer::handlePing([[maybe_unused]] const json& params) {
    return json::object();
}

void McpServer::run() {
    running_.store(true, std::memory_order_relaxed);
    fprintf(stderr, "[MCPP] Server run() started\n");

    while (running_.load(std::memory_order_relaxed) && transport_.isConnected()) {
        auto msg = transport_.readMessage(100); // 100ms timeout for responsiveness
        if (!msg.has_value()) {
            continue;
        }

        fprintf(stderr, "[MCPP] Dispatching message: %s\n", msg->dump().c_str());
        auto response = dispatcher_.dispatch(*msg);
        fprintf(stderr, "[MCPP] Dispatch returned, has_response=%d\n", response.has_value());
        
        if (response.has_value()) {
            fprintf(stderr, "[MCPP] Sending response: %s\n", response->dump().c_str());
            if (!transport_.writeMessage(*response)) {
                fprintf(stderr, "[MCPP] Write failed!\n");
                // Write error, stop server
                break;
            }
            fprintf(stderr, "[MCPP] Response sent successfully\n");
        }
    }

    fprintf(stderr, "[MCPP] Server run() exiting\n");
    running_.store(false, std::memory_order_relaxed);
}

void McpServer::start() {
    if (server_thread_ && server_thread_->joinable()) {
        return; // Already running
    }
    server_thread_ = std::make_unique<std::thread>([this]() {
        run();
    });
}

void McpServer::join() {
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
        server_thread_.reset();
    }
}

void McpServer::log(LogLevel level, const std::string& data) {
    if (!initialized_.load(std::memory_order_relaxed)) {
        return; // Don't send logs before initialization
    }
    if (level < min_log_level_) {
        return; // Below minimum level
    }

    LoggingParams params;
    params.level = level;
    params.data = data;

    json notification = JsonRpcDispatcher::makeNotification(
        "notifications/message", params.to_json()
    );
    transport_.enqueueMessage(std::move(notification));
    (void)transport_.flushQueue();
}

void McpServer::notify(const std::string& method, json params) {
    if (!initialized_.load(std::memory_order_relaxed)) {
        return; // Don't send notifications before initialization
    }
    json notification = JsonRpcDispatcher::makeNotification(method, std::move(params));
    transport_.enqueueMessage(std::move(notification));
    (void)transport_.flushQueue();
}

} // namespace mcpp
