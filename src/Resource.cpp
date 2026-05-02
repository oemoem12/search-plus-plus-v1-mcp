#include <mcpp/Resource.h>
#include <stdexcept>

namespace mcpp {

void ResourceRegistry::registerResource(ResourceDef def, ResourceReadHandler handler) {
    ResourceEntry entry;
    entry.def = std::move(def);
    entry.handler = std::move(handler);
    resources_[entry.def.uri] = std::move(entry);
}

void ResourceRegistry::registerStaticResource(std::string uri, std::string name,
                                                std::string content, std::string mimeType) {
    ResourceDef def;
    def.uri = uri;
    def.name = std::move(name);
    def.mimeType = std::move(mimeType);

    // Capture all needed values by value in the handler
    const std::string res_uri = def.uri;
    auto handler = [c = std::move(content), mt = def.mimeType, uri = res_uri](const std::string&) -> ResourceContent {
        ResourceContent rc;
        rc.uri = uri;
        rc.text = c;
        rc.mimeType = mt;
        return rc;
    };

    ResourceEntry entry;
    entry.def = std::move(def);
    entry.handler = std::move(handler);
    resources_[entry.def.uri] = std::move(entry);
}

json ResourceRegistry::listResources() const {
    json result = json::array();
    for (const auto& [uri, entry] : resources_) {
        (void)uri;
        result.push_back(entry.def.to_json());
    }
    return result;
}

json ResourceRegistry::readResource(const std::string& uri) const {
    auto it = resources_.find(uri);
    if (it == resources_.end()) {
        throw std::runtime_error("Resource not found: " + uri);
    }

    try {
        ResourceContent content = it->second.handler(uri);
        json result = json::array();
        result.push_back(content.to_json());
        return result;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Error reading resource: ") + e.what());
    }
}

bool ResourceRegistry::hasResource(const std::string& uri) const {
    return resources_.find(uri) != resources_.end();
}

bool ResourceRegistry::removeResource(const std::string& uri) {
    return resources_.erase(uri) > 0;
}

} // namespace mcpp
