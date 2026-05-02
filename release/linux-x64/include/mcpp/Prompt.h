#ifndef MCPP_PROMPT_H
#define MCPP_PROMPT_H

#include <mcpp/Types.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace mcpp {

/**
 * @brief Prompt registry for MCP prompts.
 *
 * Manages prompt definitions and handlers,
 * provides listPrompts and getPrompts functionality.
 */
class PromptRegistry {
public:
    PromptRegistry() = default;
    ~PromptRegistry() = default;

    /// Non-copyable
    PromptRegistry(const PromptRegistry&) = delete;
    PromptRegistry& operator=(const PromptRegistry&) = delete;

    /**
     * @brief Register a new prompt with its handler.
     * @param def The prompt definition (name, description, arguments).
     * @param handler The function to execute when the prompt is requested.
     */
    void registerPrompt(PromptDef def, PromptHandler handler);

    /**
     * @brief Convenience method to add a prompt with a simple handler.
     * @param name Prompt name.
     * @param description Prompt description.
     * @param handler Prompt handler.
     */
    void addPrompt(const std::string& name, const std::string& description, PromptHandler handler);

    /**
     * @brief Get all registered prompt definitions.
     * @return Array of PromptDef JSON objects.
     */
    [[nodiscard]] json listPrompts() const;

    /**
     * @brief Get a prompt by name with arguments.
     * @param name Prompt name.
     * @param args Key-value arguments for the prompt template.
     * @return PromptResult as JSON.
     */
    [[nodiscard]] PromptResult getPrompt(const std::string& name,
                                          const std::unordered_map<std::string, std::string>& args) const;

    /**
     * @brief Check if a prompt exists.
     */
    [[nodiscard]] bool hasPrompt(const std::string& name) const;

    /**
     * @brief Remove a registered prompt.
     */
    bool removePrompt(const std::string& name);

    /**
     * @brief Get the number of registered prompts.
     */
    [[nodiscard]] size_t promptCount() const noexcept { return prompts_.size(); }

private:
    struct PromptEntry {
        PromptDef def;
        PromptHandler handler;
    };

    std::unordered_map<std::string, PromptEntry> prompts_;
};

// ============================================================================
// Inline implementations
// ============================================================================

inline json PromptArgument::to_json() const noexcept {
    json j;
    j["name"] = name;
    if (description) j["description"] = *description;
    j["required"] = required;
    return j;
}

inline json PromptMessage::Content::to_json() const noexcept {
    return json{
        {"type", type},
        {"text", text}
    };
}

inline json PromptMessage::to_json() const noexcept {
    return json{
        {"role", role},
        {"content", content.to_json()}
    };
}

inline json PromptDef::to_json() const noexcept {
    json j;
    j["name"] = name;
    if (description) j["description"] = *description;
    json args = json::array();
    for (const auto& arg : arguments) {
        args.push_back(arg.to_json());
    }
    j["arguments"] = std::move(args);
    return j;
}

inline json PromptResult::to_json() const noexcept {
    json j;
    if (description) j["description"] = *description;
    json msgs = json::array();
    for (const auto& msg : messages) {
        msgs.push_back(msg.to_json());
    }
    j["messages"] = std::move(msgs);
    return j;
}

} // namespace mcpp

#endif // MCPP_PROMPT_H
