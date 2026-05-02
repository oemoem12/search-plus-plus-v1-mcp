#include <mcpp/Prompt.h>
#include <stdexcept>
#include <algorithm>

namespace mcpp {

void PromptRegistry::registerPrompt(PromptDef def, PromptHandler handler) {
    PromptEntry entry;
    entry.def = std::move(def);
    entry.handler = std::move(handler);
    prompts_[entry.def.name] = std::move(entry);
}

void PromptRegistry::addPrompt(const std::string& name, const std::string& description, PromptHandler handler) {
    PromptDef def;
    def.name = name;
    def.description = description;

    PromptEntry entry;
    entry.def = std::move(def);
    entry.handler = std::move(handler);
    prompts_[entry.def.name] = std::move(entry);
}

json PromptRegistry::listPrompts() const {
    json result = json::array();
    for (const auto& [name, entry] : prompts_) {
        (void)name;
        result.push_back(entry.def.to_json());
    }
    return result;
}

PromptResult PromptRegistry::getPrompt(const std::string& name,
                                         const std::unordered_map<std::string, std::string>& args) const {
    auto it = prompts_.find(name);
    if (it == prompts_.end()) {
        throw std::runtime_error("Prompt not found: " + name);
    }

    // Validate required arguments
    for (const auto& arg : it->second.def.arguments) {
        if (arg.required && args.find(arg.name) == args.end()) {
            throw std::runtime_error("Missing required argument: " + arg.name);
        }
    }

    try {
        return it->second.handler(args);
    } catch (const std::runtime_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Error executing prompt: ") + e.what());
    }
}

bool PromptRegistry::hasPrompt(const std::string& name) const {
    return prompts_.find(name) != prompts_.end();
}

bool PromptRegistry::removePrompt(const std::string& name) {
    return prompts_.erase(name) > 0;
}

} // namespace mcpp
