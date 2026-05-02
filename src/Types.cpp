#include <mcpp/Types.hpp>

namespace mcpp {

// ============================================================================
// JsonRpcError
// ============================================================================

json JsonRpcError::to_json() const noexcept {
    json j;
    j["code"] = code;
    j["message"] = message;
    j["data"] = data;
    return j;
}

JsonRpcError JsonRpcError::from_json(const json& j) {
    JsonRpcError err;
    err.code = j.at("code").get<int>();
    err.message = j.at("message").get<std::string>();
    if (j.contains("data")) {
        err.data = j.at("data");
    }
    return err;
}

// ============================================================================
// JsonRpcRequest
// ============================================================================

json JsonRpcRequest::to_json() const noexcept {
    json j;
    j["jsonrpc"] = jsonrpc;
    j["method"] = method;
    j["params"] = params;
    if (std::holds_alternative<std::monostate>(id)) {
        j["id"] = nullptr;
    } else if (std::holds_alternative<int64_t>(id)) {
        j["id"] = std::get<int64_t>(id);
    } else {
        j["id"] = std::get<std::string>(id);
    }
    return j;
}

JsonRpcRequest JsonRpcRequest::from_json(const json& j) {
    JsonRpcRequest req;
    req.method = j.at("method").get<std::string>();
    if (j.contains("params")) {
        req.params = j.at("params");
    }
    if (j.contains("id")) {
        const auto& id_val = j.at("id");
        if (id_val.is_null()) {
            req.id = std::monostate{};
        } else if (id_val.is_number_integer()) {
            req.id = id_val.get<int64_t>();
        } else if (id_val.is_string()) {
            req.id = id_val.get<std::string>();
        } else {
            req.id = std::monostate{};
        }
    } else {
        req.id = std::monostate{};
    }
    return req;
}

// ============================================================================
// JsonRpcResponse
// ============================================================================

json JsonRpcResponse::to_json() const noexcept {
    json j;
    j["jsonrpc"] = jsonrpc;
    j["result"] = result;
    if (std::holds_alternative<int64_t>(id)) {
        j["id"] = std::get<int64_t>(id);
    } else {
        j["id"] = std::get<std::string>(id);
    }
    return j;
}

JsonRpcResponse JsonRpcResponse::from_json(const json& j) {
    JsonRpcResponse resp;
    const auto& id_val = j.at("id");
    if (id_val.is_number_integer()) {
        resp.id = id_val.get<int64_t>();
    } else if (id_val.is_string()) {
        resp.id = id_val.get<std::string>();
    }
    if (j.contains("result")) {
        resp.result = j.at("result");
    }
    return resp;
}

// ============================================================================
// JsonRpcErrorResponse
// ============================================================================

json JsonRpcErrorResponse::to_json() const noexcept {
    json j;
    j["jsonrpc"] = jsonrpc;
    j["error"] = error.to_json();
    if (std::holds_alternative<int64_t>(id)) {
        j["id"] = std::get<int64_t>(id);
    } else {
        j["id"] = std::get<std::string>(id);
    }
    return j;
}

JsonRpcErrorResponse JsonRpcErrorResponse::from_json(const json& j) {
    JsonRpcErrorResponse resp;
    const auto& id_val = j.at("id");
    if (id_val.is_number_integer()) {
        resp.id = id_val.get<int64_t>();
    } else if (id_val.is_string()) {
        resp.id = id_val.get<std::string>();
    }
    if (j.contains("error")) {
        resp.error = JsonRpcError::from_json(j.at("error"));
    }
    return resp;
}

// ============================================================================
// ServerCapabilities
// ============================================================================

json ServerCapabilities::to_json() const noexcept {
    json j = json::object();
    if (tools) {
        json tc = json::object();
        tc["listChanged"] = tools->listChanged;
        j["tools"] = std::move(tc);
    }
    if (resources) {
        json rc = json::object();
        rc["subscribe"] = resources->subscribe;
        rc["listChanged"] = resources->listChanged;
        j["resources"] = std::move(rc);
    }
    if (prompts) {
        json pc = json::object();
        pc["listChanged"] = prompts->listChanged;
        j["prompts"] = std::move(pc);
    }
    if (!experimental.empty()) {
        j["experimental"] = experimental;
    }
    return j;
}

// ============================================================================
// InitializeParams
// ============================================================================

InitializeParams InitializeParams::from_json(const json& j) {
    InitializeParams params;
    params.protocolVersion = j.at("protocolVersion").get<std::string>();
    if (j.contains("clientInfo")) {
        params.clientInfo.name = j.at("clientInfo").value("name", "");
        params.clientInfo.version = j.at("clientInfo").value("version", "");
    }
    if (j.contains("capabilities")) {
        params.capabilities = j.at("capabilities");
    }
    return params;
}

// ============================================================================
// InitializeResult
// ============================================================================

json InitializeResult::to_json() const noexcept {
    json j;
    j["protocolVersion"] = protocolVersion;
    j["capabilities"] = capabilities.to_json();
    j["serverInfo"] = json{
        {"name", serverInfo.name},
        {"version", serverInfo.version}
    };
    if (instructions) {
        j["instructions"] = *instructions;
    }
    return j;
}

// ============================================================================
// LoggingParams
// ============================================================================

json LoggingParams::to_json() const noexcept {
    json j;
    j["level"] = level;
    j["data"] = data;
    return j;
}

} // namespace mcpp
