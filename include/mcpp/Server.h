#ifndef MCPP_SERVER_H
#define MCPP_SERVER_H

#include <mcpp/Types.hpp>
#include <mcpp/Transport.h>
#include <mcpp/JsonRpc.h>
#include <mcpp/Tool.h>
#include <mcpp/Resource.h>
#include <mcpp/Prompt.h>

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <memory>

namespace mcpp {

/**
 * @brief High-performance MCP Server.
 *
 * Implements the complete MCP protocol over stdio transport:
 * - initialize / initialized handshake
 * - tools/list, tools/call
 * - resources/list, resources/read
 * - prompts/list, prompts/get
 * - logging/setLevel
 * - notifications/initialized, notifications/message
 *
 * Usage:
 *   McpServer server("my-server", "1.0.0");
 *   server.addTool(...);
 *   server.addResource(...);
 *   server.addPrompt(...);
 *   server.run(); // blocks until stdin closes
 */
class McpServer {
public:
    /**
     * @brief Construct a new MCP server.
     * @param name Server name (reported during initialize).
     * @param version Server version (reported during initialize).
     * @param instructions Optional server instructions sent during initialize.
     */
    explicit McpServer(std::string name, std::string version,
                       std::optional<std::string> instructions = std::nullopt);

    ~McpServer() = default;

    /// Non-copyable
    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    // ==========================================================================
    // Registration APIs
    // ==========================================================================

    /**
     * @brief Register a tool.
     */
    void addTool(ToolDef def, ToolHandler handler) {
        tools_.registerTool(std::move(def), std::move(handler));
        updateCapabilities();
    }

    /**
     * @brief Register a tool with builder pattern.
     * Returns reference to ToolDef for chaining addProperty()/requireProperty().
     */
    ToolDef& addTool(const std::string& name, const std::string& description, ToolHandler handler) {
        auto& def = tools_.addTool(name, description, std::move(handler));
        updateCapabilities();
        return def;
    }

    /**
     * @brief Add a property to the last registered tool.
     */
    void addProperty(const std::string& property_name, json schema) {
        tools_.addProperty(property_name, std::move(schema));
    }

    /**
     * @brief Mark a property as required in the last registered tool.
     */
    void requireProperty(const std::string& property_name) {
        tools_.requireProperty(property_name);
    }

    /**
     * @brief Register a resource.
     */
    void addResource(ResourceDef def, ResourceReadHandler handler) {
        resources_.registerResource(std::move(def), std::move(handler));
        updateCapabilities();
    }

    /**
     * @brief Register a static text resource.
     */
    void addStaticResource(std::string uri, std::string name,
                           std::string content, std::string mimeType = "text/plain") {
        resources_.registerStaticResource(std::move(uri), std::move(name),
                                           std::move(content), std::move(mimeType));
        updateCapabilities();
    }

    /**
     * @brief Register a prompt.
     */
    void addPrompt(PromptDef def, PromptHandler handler) {
        prompts_.registerPrompt(std::move(def), std::move(handler));
        updateCapabilities();
    }

    /**
     * @brief Register a prompt with convenience method.
     */
    void addPrompt(const std::string& name, const std::string& description, PromptHandler handler) {
        prompts_.addPrompt(name, description, std::move(handler));
        updateCapabilities();
    }

    // ==========================================================================
    // Runtime
    // ==========================================================================

    /**
     * @brief Start the server event loop (blocking).
     * Reads from stdin, processes JSON-RPC messages, writes to stdout.
     * Returns when stdin is closed or a fatal error occurs.
     */
    void run();

    /**
     * @brief Start the server in a background thread (non-blocking).
     * Call join() to wait for completion.
     */
    void start();

    /**
     * @brief Wait for the server thread to finish.
     */
    void join();

    /**
     * @brief Stop the server gracefully.
     */
    void stop() noexcept { running_.store(false, std::memory_order_relaxed); }

    /**
     * @brief Check if the server is running.
     */
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(std::memory_order_relaxed); }

    /**
     * @brief Send a logging notification to the client.
     */
    void log(LogLevel level, const std::string& data);

    /**
     * @brief Send a custom notification to the client.
     */
    void notify(const std::string& method, json params = json::object());

    /**
     * @brief Get the underlying transport for custom operations.
     */
    [[nodiscard]] Transport& transport() noexcept { return transport_; }
    [[nodiscard]] const Transport& transport() const noexcept { return transport_; }

    /**
     * @brief Access the tool registry directly.
     */
    [[nodiscard]] ToolRegistry& tools() noexcept { return tools_; }
    [[nodiscard]] const ToolRegistry& tools() const noexcept { return tools_; }

    /**
     * @brief Access the resource registry directly.
     */
    [[nodiscard]] ResourceRegistry& resources() noexcept { return resources_; }
    [[nodiscard]] const ResourceRegistry& resources() const noexcept { return resources_; }

    /**
     * @brief Access the prompt registry directly.
     */
    [[nodiscard]] PromptRegistry& prompts() noexcept { return prompts_; }
    [[nodiscard]] const PromptRegistry& prompts() const noexcept { return prompts_; }

    /**
     * @brief Set custom error handler.
     */
    void setErrorHandler(std::function<void(const json&)> handler) {
        dispatcher_.setErrorHandler(std::move(handler));
    }

    /**
     * @brief Set custom default handler for unregistered methods.
     */
    void setDefaultHandler(JsonRpcDispatcher::RequestHandler handler) {
        dispatcher_.setDefaultHandler(std::move(handler));
    }

private:
    /// Initialize the internal JSON-RPC dispatcher with all standard MCP methods
    void setupHandlers();

    /// Update capabilities based on registered components
    void updateCapabilities();

    /// Handle the "initialize" request
    [[nodiscard]] json handleInitialize(const json& params);

    /// Handle the "initialized" notification
    void handleInitialized(const json& params);

    /// Handle "tools/list" request
    [[nodiscard]] json handleToolsList(const json& params);

    /// Handle "tools/call" request
    [[nodiscard]] json handleToolsCall(const json& params);

    /// Handle "resources/list" request
    [[nodiscard]] json handleResourcesList(const json& params);

    /// Handle "resources/read" request
    [[nodiscard]] json handleResourcesRead(const json& params);

    /// Handle "prompts/list" request
    [[nodiscard]] json handlePromptsList(const json& params);

    /// Handle "prompts/get" request
    [[nodiscard]] json handlePromptsGet(const json& params);

    /// Handle "logging/setLevel" notification
    void handleLoggingSetLevel(const json& params);

    /// Handle "ping" request
    [[nodiscard]] json handlePing(const json& params);

    // Server identity
    std::string server_name_;
    std::string server_version_;
    std::optional<std::string> instructions_;

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    LogLevel min_log_level_ = LogLevel::debug;

    // Components
    Transport transport_;
    JsonRpcDispatcher dispatcher_;
    ToolRegistry tools_;
    ResourceRegistry resources_;
    PromptRegistry prompts_;
    ServerCapabilities capabilities_;

    // Server thread (for async mode)
    std::unique_ptr<std::thread> server_thread_;
};

} // namespace mcpp

#endif // MCPP_SERVER_H
