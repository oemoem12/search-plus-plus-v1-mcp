#ifndef MCPP_JSONRPC_H
#define MCPP_JSONRPC_H

#include <mcpp/Types.hpp>
#include <string>
#include <optional>
#include <functional>
#include <unordered_map>

namespace mcpp {

/**
 * @brief JSON-RPC 2.0 message dispatcher and handler.
 *
 * Handles routing of JSON-RPC requests to registered handlers,
 * manages response building, and provides error handling.
 */
class JsonRpcDispatcher {
public:
    using RequestHandler = std::function<json(const json& params)>;

    JsonRpcDispatcher() = default;
    ~JsonRpcDispatcher() = default;

    /// Non-copyable
    JsonRpcDispatcher(const JsonRpcDispatcher&) = delete;
    JsonRpcDispatcher& operator=(const JsonRpcDispatcher&) = delete;

    /**
     * @brief Register a request handler for a specific method.
     * @param method The JSON-RPC method name.
     * @param handler The function to call when the method is invoked.
     */
    void registerMethod(std::string method, RequestHandler handler);

    /**
     * @brief Register a notification handler for a specific method.
     * @param method The JSON-RPC method name.
     * @param handler The function to call when the notification is received.
     */
    void registerNotification(std::string method, std::function<void(const json& params)> handler);

    /**
     * @brief Dispatch a parsed JSON-RPC request and get the response.
     * @param msg The raw JSON message.
     * @return Optional response JSON. Empty if the message was a notification
     *         or if the message was malformed (error response is returned via error_handler).
     */
    [[nodiscard]] std::optional<json> dispatch(const json& msg);

    /**
     * @brief Set a handler for JSON-RPC errors.
     */
    void setErrorHandler(std::function<void(const json& error_response)> handler) {
        error_handler_ = std::move(handler);
    }

    /**
     * @brief Set a default handler for unregistered methods.
     */
    void setDefaultHandler(RequestHandler handler) {
        default_handler_ = std::move(handler);
    }

    /**
     * @brief Create a JSON-RPC error response.
     */
    [[nodiscard]] static json makeErrorResponse(int64_t id, int code, const std::string& message,
                                                 json data = json::object()) noexcept;

    [[nodiscard]] static json makeErrorResponse(const std::string& id, int code, const std::string& message,
                                                 json data = json::object()) noexcept;

    /**
     * @brief Create a JSON-RPC success response.
     */
    [[nodiscard]] static json makeSuccessResponse(int64_t id, json result) noexcept;

    [[nodiscard]] static json makeSuccessResponse(const std::string& id, json result) noexcept;

    /**
     * @brief Create a JSON-RPC notification (no id, no response expected).
     */
    [[nodiscard]] static json makeNotification(const std::string& method, json params = json::object()) noexcept;

    /**
     * @brief Parse a JSON-RPC request from raw JSON.
     * @return JsonRpcRequest if valid, empty optional on parse error.
     */
    [[nodiscard]] static std::optional<JsonRpcRequest> parseRequest(const json& j) noexcept;

    /**
     * @brief Check if a JSON message is a valid JSON-RPC message.
     */
    [[nodiscard]] static bool isValidJsonRpc(const json& j) noexcept;

    /**
     * @brief Extract the id field as int64_t if present.
     */
    [[nodiscard]] static std::optional<int64_t> getIdAsInt(const json& j) noexcept;

    /**
     * @brief Extract the id field as string if present.
     */
    [[nodiscard]] static std::optional<std::string> getIdAsString(const json& j) noexcept;

private:
    std::unordered_map<std::string, RequestHandler> method_handlers_;
    std::unordered_map<std::string, std::function<void(const json&)>> notification_handlers_;
    std::function<void(const json&)> error_handler_;
    RequestHandler default_handler_;
};

} // namespace mcpp

#endif // MCPP_JSONRPC_H
