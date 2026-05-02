#ifndef MCPP_TOOL_H
#define MCPP_TOOL_H

#include <mcpp/Types.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace mcpp {

/**
 * @brief Tool registry for MCP tools.
 *
 * Manages tool definitions and handlers, provides
 * listTools and callTools functionality.
 */
class ToolRegistry {
public:
    ToolRegistry() = default;
    ~ToolRegistry() = default;

    /// Non-copyable
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    /**
     * @brief Register a new tool with its handler.
     * @param def The tool definition (name, description, schema).
     * @param handler The function to execute when the tool is called.
     */
    void registerTool(ToolDef def, ToolHandler handler);

    /**
     * @brief Register a tool with a builder pattern for convenience.
     * @param name Tool name.
     * @param description Tool description.
     * @param handler Tool handler.
     * @return Reference to ToolDef for chaining (adding properties).
     */
    ToolDef& addTool(const std::string& name, const std::string& description, ToolHandler handler);

    /**
     * @brief Add a property to the last registered tool's input schema.
     * @param property_name The property name.
     * @param schema The JSON schema for this property.
     */
    void addProperty(const std::string& property_name, json schema);

    /**
     * @brief Mark a property as required in the last registered tool's schema.
     */
    void requireProperty(const std::string& property_name);

    /**
     * @brief Get all registered tool definitions.
     * @return Array of ToolDef JSON objects.
     */
    [[nodiscard]] json listTools() const;

    /**
     * @brief Call a tool by name with the given parameters.
     * @param name Tool name.
     * @param params Tool parameters (must match the tool's inputSchema).
     * @return ToolResult as JSON.
     */
    [[nodiscard]] ToolResult callTool(const std::string& name, const json& params) const;

    /**
     * @brief Check if a tool exists.
     */
    [[nodiscard]] bool hasTool(const std::string& name) const;

    /**
     * @brief Remove a registered tool.
     */
    bool removeTool(const std::string& name);

    /**
     * @brief Get the number of registered tools.
     */
    [[nodiscard]] size_t toolCount() const noexcept { return tools_.size(); }

private:
    struct ToolEntry {
        ToolDef def;
        ToolHandler handler;
    };

    std::unordered_map<std::string, ToolEntry> tools_;
    mutable std::string last_added_; // For builder pattern
};

// ============================================================================
// Inline implementation for small helpers
// ============================================================================

inline json ToolInputSchema::to_json() const noexcept {
    json j;
    j["type"] = type;
    json props = json::object();
    for (const auto& [key, value] : properties) {
        props[key] = value;
    }
    j["properties"] = std::move(props);
    if (!required.empty()) {
        j["required"] = required;
    }
    return j;
}

inline json ToolDef::to_json() const noexcept {
    json j;
    j["name"] = name;
    j["description"] = description;
    j["inputSchema"] = inputSchema.to_json();
    return j;
}

inline json ToolResult::Content::to_json() const noexcept {
    json j;
    j["type"] = type;
    j["text"] = text;
    if (mimeType) j["mimeType"] = *mimeType;
    if (uri) j["uri"] = *uri;
    return j;
}

inline json ToolResult::to_json() const noexcept {
    json j;
    json contents = json::array();
    for (const auto& c : content) {
        contents.push_back(c.to_json());
    }
    j["content"] = std::move(contents);
    j["isError"] = isError;
    return j;
}

} // namespace mcpp

#endif // MCPP_TOOL_H
