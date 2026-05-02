#ifndef MCPP_RESOURCE_H
#define MCPP_RESOURCE_H

#include <mcpp/Types.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace mcpp {

/**
 * @brief Resource registry for MCP resources.
 *
 * Manages resource definitions and read handlers,
 * provides listResources and readResources functionality.
 */
class ResourceRegistry {
public:
    ResourceRegistry() = default;
    ~ResourceRegistry() = default;

    /// Non-copyable
    ResourceRegistry(const ResourceRegistry&) = delete;
    ResourceRegistry& operator=(const ResourceRegistry&) = delete;

    /**
     * @brief Register a new resource with its read handler.
     * @param def The resource definition (uri, name, description, mimeType).
     * @param handler The function to call when the resource is read.
     */
    void registerResource(ResourceDef def, ResourceReadHandler handler);

    /**
     * @brief Register a simple text resource with a static content.
     * @param uri Resource URI.
     * @param name Resource name.
     * @param content The static text content.
     * @param mimeType Optional MIME type.
     */
    void registerStaticResource(std::string uri, std::string name,
                                std::string content, std::string mimeType = "text/plain");

    /**
     * @brief Get all registered resource definitions.
     * @return Array of ResourceDef JSON objects.
     */
    [[nodiscard]] json listResources() const;

    /**
     * @brief Read a resource by URI.
     * @param uri Resource URI.
     * @return ResourceContent as JSON array (may contain multiple contents for templates).
     */
    [[nodiscard]] json readResource(const std::string& uri) const;

    /**
     * @brief Check if a resource exists.
     */
    [[nodiscard]] bool hasResource(const std::string& uri) const;

    /**
     * @brief Remove a registered resource.
     */
    bool removeResource(const std::string& uri);

    /**
     * @brief Get the number of registered resources.
     */
    [[nodiscard]] size_t resourceCount() const noexcept { return resources_.size(); }

private:
    struct ResourceEntry {
        ResourceDef def;
        ResourceReadHandler handler;
    };

    std::unordered_map<std::string, ResourceEntry> resources_;
};

// ============================================================================
// Inline implementations
// ============================================================================

inline json ResourceDef::to_json() const noexcept {
    json j;
    j["uri"] = uri;
    j["name"] = name;
    if (description) j["description"] = *description;
    if (mimeType) j["mimeType"] = *mimeType;
    return j;
}

inline json ResourceContent::to_json() const noexcept {
    json j;
    j["uri"] = uri;
    if (mimeType) j["mimeType"] = *mimeType;
    if (text) {
        j["text"] = *text;
    } else if (blob) {
        j["blob"] = *blob;
    }
    return j;
}

} // namespace mcpp

#endif // MCPP_RESOURCE_H
