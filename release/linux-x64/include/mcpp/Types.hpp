#ifndef MCPP_TYPES_HPP
#define MCPP_TYPES_HPP

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <functional>
#include <memory>
#include <unordered_map>
#include <any>
#include <cstdint>

namespace mcpp {

using json = nlohmann::json;

// ============================================================================
// JSON-RPC 2.0 Types
// ============================================================================

/// JSON-RPC 2.0 error object
struct JsonRpcError {
    int code;
    std::string message;
    json data = json::object();

    [[nodiscard]] json to_json() const noexcept;
    [[nodiscard]] static JsonRpcError from_json(const json& j);

    // Standard error codes
    static constexpr int kParseError = -32700;
    static constexpr int kInvalidRequest = -32600;
    static constexpr int kMethodNotFound = -32601;
    static constexpr int kInvalidParams = -32602;
    static constexpr int kInternalError = -32603;
};

/// JSON-RPC request
struct JsonRpcRequest {
    json jsonrpc = "2.0";
    std::string method;
    json params = json::object();
    std::variant<std::monostate, int64_t, std::string> id; // null for notifications

    [[nodiscard]] json to_json() const noexcept;
    [[nodiscard]] static JsonRpcRequest from_json(const json& j);
    [[nodiscard]] bool is_notification() const noexcept { return std::holds_alternative<std::monostate>(id); }
};

/// JSON-RPC response (success)
struct JsonRpcResponse {
    json jsonrpc = "2.0";
    std::variant<int64_t, std::string> id;
    json result = json::object();

    [[nodiscard]] json to_json() const noexcept;
    [[nodiscard]] static JsonRpcResponse from_json(const json& j);
};

/// JSON-RPC error response
struct JsonRpcErrorResponse {
    json jsonrpc = "2.0";
    std::variant<int64_t, std::string> id;
    JsonRpcError error;

    [[nodiscard]] json to_json() const noexcept;
    [[nodiscard]] static JsonRpcErrorResponse from_json(const json& j);
};

/// Unified JSON-RPC message
using JsonRpcMessage = std::variant<JsonRpcRequest, JsonRpcResponse, JsonRpcErrorResponse>;

// ============================================================================
// MCP Protocol Types
// ============================================================================

/// Client information
struct ClientInfo {
    std::string name;
    std::string version;
};

/// Server information
struct ServerInfo {
    std::string name;
    std::string version;
};

/// Implementation capabilities
struct ServerCapabilities {
    struct ToolsCapability {
        bool listChanged = false;
    };
    struct ResourcesCapability {
        bool subscribe = false;
        bool listChanged = false;
    };
    struct PromptsCapability {
        bool listChanged = false;
    };

    std::optional<ToolsCapability> tools;
    std::optional<ResourcesCapability> resources;
    std::optional<PromptsCapability> prompts;
    json experimental = json::object();

    [[nodiscard]] json to_json() const noexcept;
};

/// Initialize request params
struct InitializeParams {
    std::string protocolVersion;
    ClientInfo clientInfo;
    json capabilities = json::object();

    [[nodiscard]] static InitializeParams from_json(const json& j);
};

/// Initialize result
struct InitializeResult {
    std::string protocolVersion = "2024-11-05";
    ServerCapabilities capabilities;
    ServerInfo serverInfo;
    std::optional<std::string> instructions;

    [[nodiscard]] json to_json() const noexcept;
};

// ============================================================================
// Tool Types
// ============================================================================

/// JSON Schema property for tool arguments
struct ToolInputSchema {
    std::string type = "object";
    std::unordered_map<std::string, json> properties;
    std::vector<std::string> required;

    [[nodiscard]] json to_json() const noexcept;
};

/// Tool definition
struct ToolDef {
    std::string name;
    std::string description;
    ToolInputSchema inputSchema;
    json metadata = json::object();

    [[nodiscard]] json to_json() const noexcept;
};

/// Tool call result
struct ToolResult {
    struct Content {
        std::string type = "text"; // text, image, resource
        std::string text;
        std::optional<std::string> mimeType; // for images
        std::optional<std::string> uri;       // for resource content

        [[nodiscard]] json to_json() const noexcept;
    };

    std::vector<Content> content;
    bool isError = false;

    [[nodiscard]] json to_json() const noexcept;
};

/// Tool handler signature: takes params json, returns ToolResult
using ToolHandler = std::function<ToolResult(const json& params)>;

// ============================================================================
// Resource Types
// ============================================================================

/// Resource definition
struct ResourceDef {
    std::string uri;
    std::string name;
    std::optional<std::string> description;
    std::optional<std::string> mimeType;

    [[nodiscard]] json to_json() const noexcept;
};

/// Resource content (text or blob)
struct ResourceContent {
    std::string uri;
    std::optional<std::string> mimeType;
    std::optional<std::string> text; // for text resources
    std::optional<std::string> blob;  // for binary resources (base64)

    [[nodiscard]] json to_json() const noexcept;
};

/// Resource handler
using ResourceReadHandler = std::function<ResourceContent(const std::string& uri)>;

// ============================================================================
// Prompt Types
// ============================================================================

/// Prompt argument definition
struct PromptArgument {
    std::string name;
    std::optional<std::string> description;
    bool required = false;

    [[nodiscard]] json to_json() const noexcept;
};

/// Prompt message
struct PromptMessage {
    std::string role; // "user" or "assistant"
    struct Content {
        std::string type = "text";
        std::string text;

        [[nodiscard]] json to_json() const noexcept;
    } content;

    [[nodiscard]] json to_json() const noexcept;
};

/// Prompt definition
struct PromptDef {
    std::string name;
    std::optional<std::string> description;
    std::vector<PromptArgument> arguments;

    [[nodiscard]] json to_json() const noexcept;
};

/// Prompt result
struct PromptResult {
    std::optional<std::string> description;
    std::vector<PromptMessage> messages;

    [[nodiscard]] json to_json() const noexcept;
};

/// Prompt handler
using PromptHandler = std::function<PromptResult(const std::unordered_map<std::string, std::string>& args)>;

// ============================================================================
// Logging Types
// ============================================================================

enum class LogLevel : int {
    debug = 0,
    info = 1,
    notice = 2,
    warning = 3,
    error = 4,
    critical = 5,
    alert = 6,
    emergency = 7
};

/// Logging notification params
struct LoggingParams {
    LogLevel level = LogLevel::info;
    std::string data; // JSON-encoded data

    [[nodiscard]] json to_json() const noexcept;
};

} // namespace mcpp

// JSON serialization for nlohmann::json
NLOHMANN_JSON_SERIALIZE_ENUM(mcpp::LogLevel, {
    {mcpp::LogLevel::debug, "debug"},
    {mcpp::LogLevel::info, "info"},
    {mcpp::LogLevel::notice, "notice"},
    {mcpp::LogLevel::warning, "warning"},
    {mcpp::LogLevel::error, "error"},
    {mcpp::LogLevel::critical, "critical"},
    {mcpp::LogLevel::alert, "alert"},
    {mcpp::LogLevel::emergency, "emergency"},
})

#endif // MCPP_TYPES_HPP
