#ifndef MCPP_SEARCH_ENGINE_H
#define MCPP_SEARCH_ENGINE_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace mcpp {

/**
 * @brief Represents a single search result item.
 */
struct SearchResult {
    std::string title;
    std::string url;
    std::string snippet;           // Summary / description
    std::string extra;             // Additional info (e.g., play count, uploader for Bilibili)

    [[nodiscard]] nlohmann::json to_json() const noexcept;
};

/**
 * @brief Represents the full result from a search engine query.
 */
struct SearchResponse {
    std::string query;
    std::string engine;
    std::vector<SearchResult> results;
    bool success = true;
    std::string error_message;     // Set when success == false

    [[nodiscard]] nlohmann::json to_json() const noexcept;
};

/**
 * @brief HTTP response structure with headers (internal use).
 */
struct HttpResponse {
    std::string body;
    std::string content_type;
    std::string charset;
    long http_code = 0;
};

/**
 * @brief SearchEngine provides HTTP-based web scraping for multiple search engines.
 *
 * Supported engines:
 *   - Baidu
 *   - Bing
 *   - Bilibili (video search)
 *
 * Uses libcurl for HTTP requests and regex for HTML parsing.
 */
class SearchEngine {
public:
    /**
     * @brief Construct a SearchEngine with default timeout settings.
     * @param timeout_seconds Connection timeout in seconds (default: 10).
     * @param max_results Maximum number of results to extract per query (default: 10).
     */
    explicit SearchEngine(int timeout_seconds = 10, int max_results = 10);

    ~SearchEngine() = default;

    // Non-copyable
    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;

    // ==========================================================================
    // Search methods
    // ==========================================================================

    /**
     * @brief Search on Baidu.
     * @param query Search query string.
     * @return SearchResponse containing results.
     */
    [[nodiscard]] SearchResponse searchBaidu(const std::string& query) const;

    /**
     * @brief Search on Bing.
     * @param query Search query string.
     * @return SearchResponse containing results.
     */
    [[nodiscard]] SearchResponse searchBing(const std::string& query) const;

    /**
     * @brief Search on Bilibili (video search).
     * @param query Search query string.
     * @return SearchResponse containing results.
     */
    [[nodiscard]] SearchResponse searchBilibili(const std::string& query) const;

    /**
     * @brief Search across multiple engines in parallel.
     * @param query Search query string.
     * @param engines List of engine names: "baidu", "bing", "bilibili".
     * @return Vector of SearchResponse, one per engine.
     */
    [[nodiscard]] std::vector<SearchResponse> searchMulti(
        const std::string& query,
        const std::vector<std::string>& engines = {"baidu", "bing"}) const;

    // ==========================================================================
    // Configuration
    // ==========================================================================

    void setTimeout(int seconds) { timeout_seconds_ = seconds; }
    [[nodiscard]] int getTimeout() const { return timeout_seconds_; }

    void setMaxResults(int max) { max_results_ = max; }
    [[nodiscard]] int getMaxResults() const { return max_results_; }

    void setUserAgent(const std::string& ua) { user_agent_ = ua; }
    [[nodiscard]] const std::string& getUserAgent() const { return user_agent_; }

private:
    /**
     * @brief Perform a raw HTTP GET request.
     * @param url Full URL to fetch.
     * @return HTTP response body as string, empty on failure.
     */
    [[nodiscard]] std::string httpGet(const std::string& url) const;
    
    /**
     * @brief Perform a raw HTTP GET request with custom Accept-Language.
     * @param url Full URL to fetch.
     * @param accept_language Accept-Language header value.
     * @return HTTP response body as string, empty on failure.
     */
    [[nodiscard]] std::string httpGet(const std::string& url, const std::string& accept_language) const;
    
    /**
     * @brief Perform HTTP GET and return full response with headers.
     */
    [[nodiscard]] HttpResponse httpGetWithHeaders(const std::string& url, const std::string& accept_language) const;

    /**
     * @brief URL-encode a string.
     * @param input Raw string.
     * @return URL-encoded string.
     */
    [[nodiscard]] static std::string urlEncode(const std::string& input);

    /**
     * @brief Decode an HTML entity (e.g., &amp; -> &).
     * @param input Encoded string.
     * @return Decoded string.
     */
    [[nodiscard]] static std::string decodeHtmlEntities(const std::string& input);

    /**
     * @brief Strip all HTML tags from a string.
     */
    [[nodiscard]] static std::string stripHtmlTags(const std::string& input);

    // Configuration
    int timeout_seconds_;
    int max_results_;
    std::string user_agent_;
};

} // namespace mcpp

#endif // MCPP_SEARCH_ENGINE_H
