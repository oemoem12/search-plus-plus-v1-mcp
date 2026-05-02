/**
 * @file main.cpp
 * @brief Example MCP server demonstrating all MCP protocol features.
 *
 * This example creates a fully functional MCP server with:
 * - Multiple tools (calculator, string manipulation, time query)
 * - Static resources
 * - Prompt templates
 * - Logging notifications
 *
 * Build:
 *   cmake -B build -DCMAKE_BUILD_TYPE=Release
 *   cmake --build build
 *
 * Run (typically managed by an MCP client via stdio):
 *   ./build/mcpp_example
 */

#include <mcpp/Server.h>
#include <mcpp/SearchEngine.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

// ============================================================================
// Tool: Calculator
// ============================================================================
mcpp::ToolResult calculatorTool(const mcpp::json& params) {
    const std::string operation = params.value("operation", "");
    const double a = params.value("a", 0.0);
    const double b = params.value("b", 0.0);

    double result = 0.0;
    if (operation == "add") {
        result = a + b;
    } else if (operation == "subtract") {
        result = a - b;
    } else if (operation == "multiply") {
        result = a * b;
    } else if (operation == "divide") {
        if (b == 0.0) {
            return mcpp::ToolResult{{{"text", "Error: Division by zero", std::nullopt, std::nullopt}}, true};
        }
        result = a / b;
    } else {
        return mcpp::ToolResult{{{"text", "Unknown operation: " + operation, std::nullopt, std::nullopt}}, true};
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << result;
    std::string text = std::to_string(a) + " " + operation + " " + std::to_string(b) + " = " + oss.str();

    return mcpp::ToolResult{{{"text", std::move(text), std::nullopt, std::nullopt}}, false};
}

// ============================================================================
// Tool: String Utilities
// ============================================================================
mcpp::ToolResult stringTool(const mcpp::json& params) {
    const std::string text = params.value("text", "");
    const std::string operation = params.value("operation", "upper");

    std::string result;
    if (operation == "upper") {
        result = text;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
    } else if (operation == "lower") {
        result = text;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
    } else if (operation == "reverse") {
        result = text;
        std::reverse(result.begin(), result.end());
    } else if (operation == "length") {
        result = "Length of '" + text + "' is " + std::to_string(text.length());
    } else {
        return mcpp::ToolResult{{{"text", "Unknown operation: " + operation, std::nullopt, std::nullopt}}, true};
    }

    return mcpp::ToolResult{{{"text", std::move(result), std::nullopt, std::nullopt}}, false};
}

// ============================================================================
// Tool: Get Current Time
// ============================================================================
mcpp::ToolResult timeTool([[maybe_unused]] const mcpp::json& params) {
    const auto now = std::chrono::system_clock::now();
    const auto time_t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&time_t, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return mcpp::ToolResult{{{"text", oss.str(), std::nullopt, std::nullopt}}, false};
}

// ============================================================================
// Tool: Baidu Search
// ============================================================================
mcpp::ToolResult searchBaiduTool(const mcpp::json& params) {
    const std::string query = params.value("query", "");
    if (query.empty()) {
        return mcpp::ToolResult{{{"text", "Error: 'query' parameter is required", std::nullopt, std::nullopt}}, true};
    }

    const int limit = params.value("limit", 10);
    mcpp::SearchEngine engine(10, std::clamp(limit, 1, 50));

    auto response = engine.searchBaidu(query);

    return mcpp::ToolResult{{{"text", response.to_json().dump(2), std::nullopt, std::nullopt}}, !response.success};
}

// ============================================================================
// Tool: Bing Search
// ============================================================================
mcpp::ToolResult searchBingTool(const mcpp::json& params) {
    const std::string query = params.value("query", "");
    if (query.empty()) {
        return mcpp::ToolResult{{{"text", "Error: 'query' parameter is required", std::nullopt, std::nullopt}}, true};
    }

    const int limit = params.value("limit", 10);
    mcpp::SearchEngine engine(10, std::clamp(limit, 1, 50));

    auto response = engine.searchBing(query);

    return mcpp::ToolResult{{{"text", response.to_json().dump(2), std::nullopt, std::nullopt}}, !response.success};
}

// ============================================================================
// Tool: Bilibili Search
// ============================================================================
mcpp::ToolResult searchBilibiliTool(const mcpp::json& params) {
    const std::string query = params.value("query", "");
    if (query.empty()) {
        return mcpp::ToolResult{{{"text", "Error: 'query' parameter is required", std::nullopt, std::nullopt}}, true};
    }

    const int limit = params.value("limit", 10);
    mcpp::SearchEngine engine(10, std::clamp(limit, 1, 50));

    auto response = engine.searchBilibili(query);

    return mcpp::ToolResult{{{"text", response.to_json().dump(2), std::nullopt, std::nullopt}}, !response.success};
}

// ============================================================================
// Tool: Multi-Engine Search
// ============================================================================
mcpp::ToolResult searchMultiTool(const mcpp::json& params) {
    const std::string query = params.value("query", "");
    if (query.empty()) {
        return mcpp::ToolResult{{{"text", "Error: 'query' parameter is required", std::nullopt, std::nullopt}}, true};
    }

    const int limit = params.value("limit", 5);
    std::vector<std::string> engines;
    if (params.contains("engines") && params["engines"].is_array()) {
        for (const auto& e : params["engines"]) {
            engines.push_back(e.get<std::string>());
        }
    } else {
        engines = {"baidu", "bing"};
    }

    mcpp::SearchEngine engine(10, std::clamp(limit, 1, 20));
    auto responses = engine.searchMulti(query, engines);

    mcpp::json combined;
    combined["query"] = query;
    mcpp::json engines_arr = mcpp::json::array();
    for (const auto& resp : responses) {
        engines_arr.push_back(resp.to_json());
    }
    combined["engines"] = std::move(engines_arr);

    bool all_failed = true;
    for (const auto& resp : responses) {
        if (resp.success) {
            all_failed = false;
            break;
        }
    }

    return mcpp::ToolResult{{{"text", combined.dump(2), std::nullopt, std::nullopt}}, all_failed};
}

// ============================================================================
// Prompt: Code Review
// ============================================================================
mcpp::PromptResult codeReviewPrompt(const std::unordered_map<std::string, std::string>& args) {
    auto it = args.find("code");
    std::string code = (it != args.end()) ? it->second : "<no code provided>";

    it = args.find("language");
    std::string language = (it != args.end()) ? it->second : "unknown";

    mcpp::PromptResult result;
    result.description = "Code review prompt for " + language + " code";
    result.messages.push_back({
        "user",
        {"text", "Please review the following " + language + " code and provide feedback:\n\n```" + language + "\n" + code + "\n```\n\nConsider: correctness, performance, security, and readability."}
    });

    return result;
}

// ============================================================================
// Prompt: Explain Concept
// ============================================================================
mcpp::PromptResult explainPrompt(const std::unordered_map<std::string, std::string>& args) {
    auto it = args.find("topic");
    std::string topic = (it != args.end()) ? it->second : "unknown topic";

    it = args.find("level");
    std::string level = (it != args.end()) ? it->second : "beginner";

    mcpp::PromptResult result;
    result.description = "Explain " + topic + " at " + level + " level";
    result.messages.push_back({
        "user",
        {"text", "Please explain the concept of '" + topic + "' at a " + level + " level. Use clear examples and avoid unnecessary jargon."}
    });

    return result;
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main() {
    // Create the MCP server with name and version
    mcpp::McpServer server("mcpp-example", "1.0.0",
        "This is a demo MCP server showcasing tools, resources, and prompts.");

    // ---- Register Tools ----

    // Tool 1: Calculator
    server.addTool("calculator", "Perform basic arithmetic operations", calculatorTool);
    server.addProperty("operation", {
        {"type", "string"},
        {"description", "The operation to perform: add, subtract, multiply, divide"},
        {"enum", {"add", "subtract", "multiply", "divide"}}
    });
    server.addProperty("a", {{"type", "number"}, {"description", "First operand"}});
    server.addProperty("b", {{"type", "number"}, {"description", "Second operand"}});
    server.requireProperty("operation");
    server.requireProperty("a");
    server.requireProperty("b");

    // Tool 2: String utilities
    server.addTool("string_util", "Manipulate strings (upper, lower, reverse, length)", stringTool);
    server.addProperty("text", {{"type", "string"}, {"description", "The input text"}});
    server.addProperty("operation", {
        {"type", "string"},
        {"description", "The operation: upper, lower, reverse, length"},
        {"enum", {"upper", "lower", "reverse", "length"}}
    });
    server.requireProperty("text");
    server.requireProperty("operation");

    // Tool 3: Current time
    server.addTool("get_time", "Get the current date and time with milliseconds", timeTool);

    // Tool 4: Baidu search
    server.addTool("search_baidu", "Search on Baidu and return results with title, URL, and snippet", searchBaiduTool);
    server.addProperty("query", {{"type", "string"}, {"description", "The search query"}});
    server.addProperty("limit", {{"type", "integer"}, {"description", "Maximum number of results (default: 10, max: 50)"}, {"minimum", 1}, {"maximum", 50}});
    server.requireProperty("query");

    // Tool 5: Bing search
    server.addTool("search_bing", "Search on Bing and return results with title, URL, and snippet", searchBingTool);
    server.addProperty("query", {{"type", "string"}, {"description", "The search query"}});
    server.addProperty("limit", {{"type", "integer"}, {"description", "Maximum number of results (default: 10, max: 50)"}, {"minimum", 1}, {"maximum", 50}});
    server.requireProperty("query");

    // Tool 6: Bilibili search
    server.addTool("search_bilibili", "Search on Bilibili for videos and return results with title, URL, view count, and uploader", searchBilibiliTool);
    server.addProperty("query", {{"type", "string"}, {"description", "The search query"}});
    server.addProperty("limit", {{"type", "integer"}, {"description", "Maximum number of results (default: 10, max: 50)"}, {"minimum", 1}, {"maximum", 50}});
    server.requireProperty("query");

    // Tool 7: Multi-engine search
    server.addTool("search_multi", "Search across multiple engines (baidu, bing, bilibili) in parallel", searchMultiTool);
    server.addProperty("query", {{"type", "string"}, {"description", "The search query"}});
    server.addProperty("engines", {{"type", "array"}, {"description", "List of engines to search: [\"baidu\", \"bing\", \"bilibili\"] (default: [\"baidu\", \"bing\"])"}, {"items", {"type", "string", "enum", {"baidu", "bing", "bilibili"}}}});
    server.addProperty("limit", {{"type", "integer"}, {"description", "Maximum number of results per engine (default: 5, max: 20)"}, {"minimum", 1}, {"maximum", 20}});
    server.requireProperty("query");

    // ---- Register Resources ----

    server.addStaticResource(
        "mcpp://docs/getting-started",
        "Getting Started Guide",
        "# Getting Started with mcpp\n\n"
        "This is a high-performance C++ MCP server library.\n\n"
        "## Features\n"
        "- JSON-RPC 2.0 protocol\n"
        "- stdio transport\n"
        "- Tool, Resource, and Prompt support\n"
        "- Thread-safe message queue\n"
        "- Content-Length framed I/O\n\n"
        "## Quick Start\n"
        "```cpp\n"
        "mcpp::McpServer server(\"my-server\", \"1.0.0\");\n"
        "server.addTool(\"hello\", \"Say hello\", ...);\n"
        "server.run();\n"
        "```",
        "text/markdown"
    );

    server.addStaticResource(
        "mcpp://docs/api-reference",
        "API Reference",
        "## McpServer\n"
        "- `addTool(name, description, handler)` - Register a tool\n"
        "- `addResource(def, handler)` - Register a resource\n"
        "- `addPrompt(name, description, handler)` - Register a prompt\n"
        "- `run()` - Start the server (blocking)\n"
        "- `log(level, data)` - Send log notification\n"
        "- `notify(method, params)` - Send custom notification\n\n"
        "## ToolHandler\n"
        "Signature: `ToolResult(const json& params)`\n\n"
        "## ResourceReadHandler\n"
        "Signature: `ResourceContent(const std::string& uri)`\n\n"
        "## PromptHandler\n"
        "Signature: `PromptResult(const unordered_map<string, string>& args)`",
        "text/markdown"
    );

    // ---- Register Prompts ----

    server.addPrompt("code_review", "Generate a code review request", codeReviewPrompt);
    {
        // Manually set arguments for the prompt definition
        // We need to access the internal def - let's re-register with full definition
    }

    server.addPrompt("explain", "Ask for an explanation of a topic at a given level", explainPrompt);

    // ---- Start the server ----

    // Run the server (blocks until stdin closes)
    // Note: Do NOT call server.log() before server.run() as it will be ignored
    // until initialization handshake is complete.
    server.run();

    return 0;
}
