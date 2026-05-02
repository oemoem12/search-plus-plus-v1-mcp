#include <mcpp/Tool.h>
#include <stdexcept>

namespace mcpp {

void ToolRegistry::registerTool(ToolDef def, ToolHandler handler) {
    ToolEntry entry;
    entry.def = std::move(def);
    entry.handler = std::move(handler);
    tools_[entry.def.name] = std::move(entry);
}

ToolDef& ToolRegistry::addTool(const std::string& name, const std::string& description, ToolHandler handler) {
    ToolDef def;
    def.name = name;
    def.description = description;

    // Register with a placeholder handler that calls the real one
    // We need to store the handler separately
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        ToolEntry entry;
        entry.def = def;
        entry.handler = std::move(handler);
        tools_[name] = std::move(entry);
        last_added_ = name;
        return tools_[name].def;
    }
    // Update existing
    it->second.def = std::move(def);
    it->second.handler = std::move(handler);
    last_added_ = name;
    return it->second.def;
}

void ToolRegistry::addProperty(const std::string& property_name, json schema) {
    if (last_added_.empty()) {
        throw std::runtime_error("No tool has been added yet. Call addTool() first.");
    }
    auto it = tools_.find(last_added_);
    if (it == tools_.end()) {
        throw std::runtime_error("Tool not found: " + last_added_);
    }
    it->second.def.inputSchema.properties[property_name] = std::move(schema);
}

void ToolRegistry::requireProperty(const std::string& property_name) {
    if (last_added_.empty()) {
        throw std::runtime_error("No tool has been added yet. Call addTool() first.");
    }
    auto it = tools_.find(last_added_);
    if (it == tools_.end()) {
        throw std::runtime_error("Tool not found: " + last_added_);
    }
    it->second.def.inputSchema.required.push_back(property_name);
}

json ToolRegistry::listTools() const {
    json result = json::array();
    for (const auto& [name, entry] : tools_) {
        (void)name;
        result.push_back(entry.def.to_json());
    }
    return result;
}

ToolResult ToolRegistry::callTool(const std::string& name, const json& params) const {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        ToolResult result;
        result.isError = true;
        result.content.push_back({"text", "Tool not found: " + name, std::nullopt, std::nullopt});
        return result;
    }

    try {
        return it->second.handler(params);
    } catch (const std::exception& e) {
        ToolResult result;
        result.isError = true;
        result.content.push_back({"text", std::string("Error executing tool: ") + e.what(), std::nullopt, std::nullopt});
        return result;
    }
}

bool ToolRegistry::hasTool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

bool ToolRegistry::removeTool(const std::string& name) {
    return tools_.erase(name) > 0;
}

} // namespace mcpp
