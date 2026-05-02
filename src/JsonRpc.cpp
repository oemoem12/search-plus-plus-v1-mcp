#include <mcpp/JsonRpc.h>

namespace mcpp {

void JsonRpcDispatcher::registerMethod(std::string method, RequestHandler handler) {
    method_handlers_[std::move(method)] = std::move(handler);
}

void JsonRpcDispatcher::registerNotification(std::string method, std::function<void(const json& params)> handler) {
    notification_handlers_[std::move(method)] = std::move(handler);
}

std::optional<json> JsonRpcDispatcher::dispatch(const json& msg) {
    // Validate JSON-RPC
    if (!isValidJsonRpc(msg)) {
        json error_resp = makeErrorResponse(
            0,
            JsonRpcError::kInvalidRequest,
            "Invalid JSON-RPC: missing or invalid jsonrpc field"
        );
        if (error_handler_) error_handler_(error_resp);
        return error_resp;
    }

    const std::string method = msg.value("method", "");
    if (method.empty()) {
        // No method field - could be a response
        if (msg.contains("result") || msg.contains("error")) {
            // This is a response, not a request
            return std::nullopt;
        }

        json error_resp = makeErrorResponse(
            0,
            JsonRpcError::kInvalidRequest,
            "Missing method field"
        );
        if (error_handler_) error_handler_(error_resp);
        return error_resp;
    }

    const bool has_id = msg.contains("id");

    // Check if this is a notification (no id)
    if (!has_id) {
        auto it = notification_handlers_.find(method);
        if (it != notification_handlers_.end()) {
            try {
                const json& params = msg.contains("params") ? msg.at("params") : json::object();
                it->second(params);
            } catch (const std::exception& e) {
                // Notifications don't return errors, but we can log
            }
        }
        return std::nullopt;
    }

    // This is a request - find and execute handler
    auto it = method_handlers_.find(method);
    if (it == method_handlers_.end()) {
        if (default_handler_) {
            try {
                const json& params = msg.contains("params") ? msg.at("params") : json::object();
                json result = default_handler_(params);
                const auto id_int = getIdAsInt(msg);
                if (id_int.has_value()) {
                    return makeSuccessResponse(*id_int, std::move(result));
                }
                const auto id_str = getIdAsString(msg);
                if (id_str.has_value()) {
                    return makeSuccessResponse(*id_str, std::move(result));
                }
            } catch (const std::exception& e) {
                const auto id_int = getIdAsInt(msg);
                if (id_int.has_value()) {
                    return makeErrorResponse(*id_int, JsonRpcError::kInternalError, e.what());
                }
                const auto id_str = getIdAsString(msg);
                if (id_str.has_value()) {
                    return makeErrorResponse(*id_str, JsonRpcError::kInternalError, e.what());
                }
            }
        }

        // Method not found
        const auto id_int = getIdAsInt(msg);
        if (id_int.has_value()) {
            json error_resp = makeErrorResponse(*id_int, JsonRpcError::kMethodNotFound,
                                                 "Method not found: " + method);
            if (error_handler_) error_handler_(error_resp);
            return error_resp;
        }
        const auto id_str = getIdAsString(msg);
        if (id_str.has_value()) {
            json error_resp = makeErrorResponse(*id_str, JsonRpcError::kMethodNotFound,
                                                 "Method not found: " + method);
            if (error_handler_) error_handler_(error_resp);
            return error_resp;
        }
        return std::nullopt;
    }

    // Execute handler
    try {
        const json& params = msg.contains("params") ? msg.at("params") : json::object();
        json result = it->second(params);

        const auto id_int = getIdAsInt(msg);
        if (id_int.has_value()) {
            return makeSuccessResponse(*id_int, std::move(result));
        }
        const auto id_str = getIdAsString(msg);
        if (id_str.has_value()) {
            return makeSuccessResponse(*id_str, std::move(result));
        }
    } catch (const std::exception& e) {
        const auto id_int = getIdAsInt(msg);
        if (id_int.has_value()) {
            json error_resp = makeErrorResponse(*id_int, JsonRpcError::kInternalError, e.what());
            if (error_handler_) error_handler_(error_resp);
            return error_resp;
        }
        const auto id_str = getIdAsString(msg);
        if (id_str.has_value()) {
            json error_resp = makeErrorResponse(*id_str, JsonRpcError::kInternalError, e.what());
            if (error_handler_) error_handler_(error_resp);
            return error_resp;
        }
    }

    return std::nullopt;
}

json JsonRpcDispatcher::makeErrorResponse(int64_t id, int code, const std::string& message,
                                            json data) noexcept {
    return json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message},
            {"data", std::move(data)}
        }}
    };
}

json JsonRpcDispatcher::makeErrorResponse(const std::string& id, int code, const std::string& message,
                                            json data) noexcept {
    return json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message},
            {"data", std::move(data)}
        }}
    };
}

json JsonRpcDispatcher::makeSuccessResponse(int64_t id, json result) noexcept {
    return json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", std::move(result)}
    };
}

json JsonRpcDispatcher::makeSuccessResponse(const std::string& id, json result) noexcept {
    return json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", std::move(result)}
    };
}

json JsonRpcDispatcher::makeNotification(const std::string& method, json params) noexcept {
    return json{
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", std::move(params)}
    };
}

std::optional<JsonRpcRequest> JsonRpcDispatcher::parseRequest(const json& j) noexcept {
    try {
        if (!j.contains("jsonrpc") || !j.contains("method")) {
            return std::nullopt;
        }
        JsonRpcRequest req;
        req.method = j.at("method").get<std::string>();
        if (j.contains("params")) {
            req.params = j.at("params");
        }
        if (j.contains("id")) {
            const auto& id = j.at("id");
            if (id.is_null()) {
                req.id = std::monostate{};
            } else if (id.is_number_integer()) {
                req.id = id.get<int64_t>();
            } else if (id.is_string()) {
                req.id = id.get<std::string>();
            } else {
                return std::nullopt;
            }
        } else {
            req.id = std::monostate{};
        }
        return req;
    } catch (...) {
        return std::nullopt;
    }
}

bool JsonRpcDispatcher::isValidJsonRpc(const json& j) noexcept {
    try {
        return j.contains("jsonrpc") && j.at("jsonrpc") == "2.0";
    } catch (...) {
        return false;
    }
}

std::optional<int64_t> JsonRpcDispatcher::getIdAsInt(const json& j) noexcept {
    try {
        if (!j.contains("id") || j.at("id").is_null()) {
            return std::nullopt;
        }
        if (j.at("id").is_number_integer()) {
            return j.at("id").get<int64_t>();
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> JsonRpcDispatcher::getIdAsString(const json& j) noexcept {
    try {
        if (!j.contains("id") || j.at("id").is_null()) {
            return std::nullopt;
        }
        if (j.at("id").is_string()) {
            return j.at("id").get<std::string>();
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace mcpp
