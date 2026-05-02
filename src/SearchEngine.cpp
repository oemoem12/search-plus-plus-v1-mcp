/**
 * @file SearchEngine.cpp
 * @brief Implementation of SearchEngine - web scraping for Baidu, Bing, Bilibili.
 *
 * Uses libcurl for HTTP requests and C++17 regex for HTML parsing.
 */

#include <mcpp/SearchEngine.h>

#include <curl/curl.h>
#include <regex>
#include <sstream>
#include <algorithm>
#include <thread>
#include <future>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <set>

namespace mcpp {

// ============================================================================
// GBK to UTF-8 conversion (simplified implementation)
// ============================================================================

/**
 * @brief Convert GBK encoded bytes to UTF-8 string.
 * 
 * GBK is a variable-length encoding:
 * - ASCII bytes (0x00-0x7F) are single byte
 * - Chinese characters are two bytes: first byte 0x81-0xFE, second byte 0x40-0xFE (except 0x7F)
 * 
 * This is a simplified conversion that handles most common GBK characters.
 */
static std::string gbkToUtf8(const std::string& gbk_str) {
    std::string result;
    result.reserve(gbk_str.size() * 3 / 2);
    
    size_t i = 0;
    while (i < gbk_str.size()) {
        unsigned char c = static_cast<unsigned char>(gbk_str[i]);
        
        // ASCII character (0x00-0x7F)
        if (c < 0x80) {
            result += static_cast<char>(c);
            ++i;
            continue;
        }
        
        // GBK two-byte character
        if (i + 1 < gbk_str.size()) {
            unsigned char c2 = static_cast<unsigned char>(gbk_str[i + 1]);
            
            // Validate GBK second byte
            if (c2 >= 0x40 && c2 <= 0xFE && c2 != 0x7F) {
                // Convert GBK to Unicode code point
                uint16_t gbk_code = (static_cast<uint16_t>(c) << 8) | c2;
                uint32_t unicode = 0;
                
                // GBK/1 (0xA1A1-0xF7FE) - corresponds to Unicode CJK Unified Ideographs
                // This is a simplified mapping for common characters
                if (gbk_code >= 0xB0A1 && gbk_code <= 0xF7FE) {
                    // Common Chinese characters range
                    // Map to Unicode CJK range (U+4E00 - U+9FFF)
                    uint16_t offset = (c - 0xB0) * 190 + (c2 - (c2 >= 0x80 ? 0x41 : 0x40));
                    unicode = 0x4E00 + offset;
                } else if (gbk_code >= 0xA1A1 && gbk_code <= 0xA9FE) {
                    // GBK symbols and punctuation
                    uint16_t offset = (c - 0xA1) * 190 + (c2 - (c2 >= 0x80 ? 0x41 : 0x40));
                    unicode = 0x3000 + offset;  // CJK Symbols and Punctuation
                } else {
                    // For other GBK codes, use a placeholder or try direct mapping
                    // This handles extended GBK characters
                    unicode = 0xFFFD;  // Replacement character
                }
                
                // Convert Unicode code point to UTF-8
                if (unicode < 0x80) {
                    result += static_cast<char>(unicode);
                } else if (unicode < 0x800) {
                    result += static_cast<char>(0xC0 | (unicode >> 6));
                    result += static_cast<char>(0x80 | (unicode & 0x3F));
                } else if (unicode < 0x10000) {
                    result += static_cast<char>(0xE0 | (unicode >> 12));
                    result += static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (unicode & 0x3F));
                } else {
                    result += static_cast<char>(0xF0 | (unicode >> 18));
                    result += static_cast<char>(0x80 | ((unicode >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (unicode & 0x3F));
                }
                
                i += 2;
                continue;
            }
        }
        
        // Invalid GBK sequence - skip this byte
        result += static_cast<char>(0xEF);  // UTF-8 replacement character
        result += static_cast<char>(0xBF);
        result += static_cast<char>(0xBD);
        ++i;
    }
    
    return result;
}

/**
 * @brief Check if a string contains valid UTF-8.
 */
static bool isValidUtf8(const std::string& str) {
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        // ASCII
        if (c < 0x80) {
            ++i;
            continue;
        }
        
        // Multi-byte UTF-8
        size_t bytes = 0;
        if ((c & 0xE0) == 0xC0) bytes = 2;        // 110xxxxx
        else if ((c & 0xF0) == 0xE0) bytes = 3;   // 1110xxxx
        else if ((c & 0xF8) == 0xF0) bytes = 4;   // 11110xxx
        else return false;  // Invalid UTF-8 start byte
        
        if (i + bytes > str.size()) return false;
        
        for (size_t j = 1; j < bytes; ++j) {
            unsigned char next = static_cast<unsigned char>(str[i + j]);
            if ((next & 0xC0) != 0x80) return false;  // Not a continuation byte
        }
        
        i += bytes;
    }
    return true;
}

/**
 * @brief Clean up potentially invalid UTF-8 string by replacing invalid sequences.
 */
static std::string cleanUtf8(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        // ASCII
        if (c < 0x80) {
            result += static_cast<char>(c);
            ++i;
            continue;
        }
        
        // Multi-byte UTF-8
        size_t bytes = 0;
        if ((c & 0xE0) == 0xC0) bytes = 2;
        else if ((c & 0xF0) == 0xE0) bytes = 3;
        else if ((c & 0xF8) == 0xF0) bytes = 4;
        else {
            // Invalid start byte - skip or replace
            ++i;
            continue;
        }
        
        if (i + bytes > str.size()) {
            // Incomplete sequence - skip remaining
            break;
        }
        
        bool valid = true;
        for (size_t j = 1; j < bytes; ++j) {
            unsigned char next = static_cast<unsigned char>(str[i + j]);
            if ((next & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }
        
        if (valid) {
            result.append(str, i, bytes);
            i += bytes;
        } else {
            ++i;
        }
    }
    
    return result;
}

// ============================================================================
// JSON serialization
// ============================================================================

nlohmann::json SearchResult::to_json() const noexcept {
    nlohmann::json j;
    // Clean UTF-8 before serialization to avoid encoding errors
    j["title"] = cleanUtf8(title);
    j["url"] = cleanUtf8(url);
    j["snippet"] = cleanUtf8(snippet);
    if (!extra.empty()) {
        j["extra"] = cleanUtf8(extra);
    }
    return j;
}

nlohmann::json SearchResponse::to_json() const noexcept {
    nlohmann::json j;
    j["query"] = cleanUtf8(query);
    j["engine"] = engine;
    j["success"] = success;
    if (!success) {
        j["error"] = cleanUtf8(error_message);
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : results) {
        arr.push_back(r.to_json());
    }
    j["results"] = std::move(arr);
    return j;
}

// ============================================================================
// SearchEngine implementation
// ============================================================================

SearchEngine::SearchEngine(int timeout_seconds, int max_results)
    : timeout_seconds_(timeout_seconds)
    , max_results_(max_results)
    , user_agent_("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36")
{
}

// ============================================================================
// HTTP GET via libcurl
// ============================================================================

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buffer = static_cast<std::string*>(userp);
    size_t total = size * nmemb;
    buffer->append(static_cast<char*>(contents), total);
    return total;
}

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    auto* response = static_cast<HttpResponse*>(userdata);
    std::string header(buffer, total);
    
    // Parse Content-Type header
    if (header.find("Content-Type:") == 0 || header.find("content-type:") == 0) {
        response->content_type = header;
        
        // Extract charset from Content-Type
        size_t charset_pos = header.find("charset=");
        if (charset_pos != std::string::npos) {
            size_t start = charset_pos + 8;
            // Skip any leading whitespace or quotes
            while (start < header.size() && (header[start] == ' ' || header[start] == '"' || header[start] == '\'')) {
                ++start;
            }
            size_t end = start;
            while (end < header.size() && header[end] != ';' && header[end] != '\r' && 
                   header[end] != '\n' && header[end] != ' ' && header[end] != '"') {
                ++end;
            }
            response->charset = header.substr(start, end - start);
        }
    }
    
    return total;
}

HttpResponse SearchEngine::httpGetWithHeaders(const std::string& url, const std::string& accept_language) const {
    HttpResponse response;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[SearchEngine] Failed to initialize libcurl\n";
        return response;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("User-Agent: " + user_agent_).c_str());
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
    headers = curl_slist_append(headers, ("Accept-Language: " + accept_language).c_str());
    headers = curl_slist_append(headers, "Accept-Encoding: identity");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds_));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(timeout_seconds_));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "[SearchEngine] HTTP request failed: " << curl_easy_strerror(res) << "\n";
        response.body.clear();
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.http_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

std::string SearchEngine::httpGet(const std::string& url) const {
    // Default to Chinese language preference for backward compatibility
    return httpGet(url, "zh-CN,zh;q=0.9,en;q=0.8");
}

std::string SearchEngine::httpGet(const std::string& url, const std::string& accept_language) const {
    HttpResponse response = httpGetWithHeaders(url, accept_language);
    
    if (response.body.empty()) {
        return {};
    }
    
    // Detect encoding from charset
    std::string charset = response.charset;
    
    // Convert charset to lowercase for comparison
    std::transform(charset.begin(), charset.end(), charset.begin(), ::tolower);
    
    // Also check for meta charset in HTML if not found in headers
    if (charset.empty()) {
        static const std::regex meta_charset_re(
            "<meta[^>]+charset=[\"']?([^\"'\\s>]+)",
            std::regex::icase
        );
        std::smatch match;
        if (std::regex_search(response.body, match, meta_charset_re)) {
            charset = match[1].str();
            std::transform(charset.begin(), charset.end(), charset.begin(), ::tolower);
        }
    }
    
    // Handle different encodings
    if (charset == "gbk" || charset == "gb2312" || charset == "gb18030") {
        // Convert GBK/GB2312/GB18030 to UTF-8
        std::cout << "[SearchEngine] Converting " << charset << " to UTF-8\n";
        return gbkToUtf8(response.body);
    }
    
    // If no charset detected and content has high bytes, check if it's valid UTF-8
    bool has_high_bytes = false;
    for (char c : response.body) {
        if (static_cast<unsigned char>(c) >= 0x80) {
            has_high_bytes = true;
            break;
        }
    }
    
    if (has_high_bytes && !isValidUtf8(response.body)) {
        // Try to detect if it's GBK encoded
        // GBK typically has bytes in range 0x81-0xFE followed by 0x40-0xFE
        bool likely_gbk = false;
        for (size_t i = 0; i + 1 < response.body.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(response.body[i]);
            unsigned char c2 = static_cast<unsigned char>(response.body[i + 1]);
            if (c >= 0x81 && c <= 0xFE && c2 >= 0x40 && c2 <= 0xFE && c2 != 0x7F) {
                likely_gbk = true;
                break;
            }
        }
        
        if (likely_gbk) {
            std::cout << "[SearchEngine] Detected GBK encoding (auto-detected)\n";
            return gbkToUtf8(response.body);
        }
        
        // Not GBK, just clean up invalid UTF-8 sequences
        std::cout << "[SearchEngine] Cleaning invalid UTF-8 sequences\n";
        return cleanUtf8(response.body);
    }
    
    return response.body;
}

// ============================================================================
// URL encoding
// ============================================================================

std::string SearchEngine::urlEncode(const std::string& input) {
    std::ostringstream oss;
    for (char c : input) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << (static_cast<int>(static_cast<unsigned char>(c)));
        }
    }
    return oss.str();
}

// ============================================================================
// HTML decoding helpers
// ============================================================================

std::string SearchEngine::decodeHtmlEntities(const std::string& input) {
    std::string result = input;

    static const std::pair<std::string, std::string> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&#39;", "'"}, {"&apos;", "'"}, {"&nbsp;", " "},
        {"&#x27;", "'"}, {"&#x2F;", "/"}, {"&#x2f;", "/"}
    };

    for (const auto& [encoded, decoded] : entities) {
        size_t pos = 0;
        while ((pos = result.find(encoded, pos)) != std::string::npos) {
            result.replace(pos, encoded.length(), decoded);
            pos += decoded.length();
        }
    }

    return result;
}

std::string SearchEngine::stripHtmlTags(const std::string& input) {
    static const std::regex tag_re("<[^>]*>");
    return std::regex_replace(input, tag_re, "");
}

// ============================================================================
// Baidu search
// ============================================================================

/**
 * @brief Check if a URL should be filtered out (images, videos, docs, etc.)
 */
static bool shouldFilterUrl(const std::string& url) {
    // Convert to lowercase for comparison
    std::string lower_url = url;
    std::transform(lower_url.begin(), lower_url.end(), lower_url.begin(), ::tolower);
    
    // Filter out image search results
    if (lower_url.find("image.baidu.com") != std::string::npos ||
        lower_url.find("images.baidu.com") != std::string::npos ||
        lower_url.find("timg.cn") != std::string::npos ||
        lower_url.find("/img/") != std::string::npos ||
        lower_url.find("p.qpic.cn") != std::string::npos ||
        lower_url.find("imgsrc.baidu.com") != std::string::npos) {
        return true;
    }
    
    // Filter out video album/playlist results
    if (lower_url.find("video.baidu.com") != std::string::npos ||
        lower_url.find("v.baidu.com") != std::string::npos ||
        lower_url.find("baidu.com/v") != std::string::npos) {
        return true;
    }
    
    // Filter out document files
    if (lower_url.find(".pdf") != std::string::npos ||
        lower_url.find(".doc") != std::string::npos ||
        lower_url.find(".docx") != std::string::npos ||
        lower_url.find(".ppt") != std::string::npos ||
        lower_url.find(".xls") != std::string::npos ||
        lower_url.find(".txt") != std::string::npos) {
        return true;
    }
    
    // Filter out Baidu internal pages that are not useful
    if (lower_url.find("baidu.com/link?url=") == std::string::npos &&
        (lower_url.find("passport.baidu.com") != std::string::npos ||
         lower_url.find("pan.baidu.com") != std::string::npos ||
         lower_url.find("wenku.baidu.com") != std::string::npos ||
         lower_url.find("zhidao.baidu.com") != std::string::npos ||
         lower_url.find("tieba.baidu.com") != std::string::npos ||
         lower_url.find("baike.baidu.com") != std::string::npos ||
         lower_url.find("jingyan.baidu.com") != std::string::npos)) {
        // Keep zhidao, baike, jingyan, tieba as they may have useful content
        // But still filter out some
    }
    
    // Filter out common media file extensions
    if (lower_url.rfind(".jpg") != std::string::npos ||
        lower_url.rfind(".jpeg") != std::string::npos ||
        lower_url.rfind(".png") != std::string::npos ||
        lower_url.rfind(".gif") != std::string::npos ||
        lower_url.rfind(".mp4") != std::string::npos ||
        lower_url.rfind(".avi") != std::string::npos ||
        lower_url.rfind(".mp3") != std::string::npos) {
        return true;
    }
    
    return false;
}

/**
 * @brief Check if a title looks like a valid article/tutorial result
 */
static bool isValidTitle(const std::string& title) {
    if (title.empty() || title.size() < 2) {
        return false;
    }
    
    // Filter out titles that are clearly image-related
    std::string lower_title = title;
    std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
    
    // Skip if title is just "图片" or similar
    if (lower_title == "图片" || lower_title == "图片搜索" || lower_title == "相关图片" ||
        lower_title.find("图片结果") != std::string::npos) {
        return false;
    }
    
    return true;
}

/**
 * @brief Extract real URL from Baidu redirect URL
 * Baidu uses redirect URLs like: https://www.baidu.com/link?url=xxxxx
 * The real URL is sometimes embedded in the URL or needs to be followed
 */
static std::string extractRealUrl(const std::string& url) {
    // If it's already a direct URL, return as-is
    if (url.find("baidu.com/link?url=") == std::string::npos) {
        return url;
    }
    
    // For Baidu redirect URLs, we keep them as-is since they will redirect
    // The filtering should happen based on the redirect target
    return url;
}

/**
 * @brief Deduplicate results by URL and title similarity
 */
static void deduplicateResults(std::vector<SearchResult>& results) {
    if (results.size() <= 1) return;
    
    std::vector<SearchResult> unique_results;
    std::set<std::string> seen_urls;
    std::set<std::string> seen_titles;
    
    for (auto& result : results) {
        // Normalize URL for comparison (remove trailing slashes, protocol)
        std::string normalized_url = result.url;
        if (!normalized_url.empty() && normalized_url.back() == '/') {
            normalized_url.pop_back();
        }
        
        // Remove protocol for comparison
        size_t proto_end = normalized_url.find("://");
        if (proto_end != std::string::npos) {
            normalized_url = normalized_url.substr(proto_end + 3);
        }
        
        // Normalize title (lowercase, remove extra spaces)
        std::string normalized_title = result.title;
        std::transform(normalized_title.begin(), normalized_title.end(), 
                      normalized_title.begin(), ::tolower);
        normalized_title.erase(std::unique(normalized_title.begin(), normalized_title.end(),
            [](char a, char b) { return std::isspace(a) && std::isspace(b); }),
            normalized_title.end());
        
        // Check if we've seen this URL or similar title
        bool is_duplicate = false;
        
        // Check URL
        if (seen_urls.find(normalized_url) != seen_urls.end()) {
            is_duplicate = true;
        }
        
        // Check title similarity (exact match for now)
        if (!is_duplicate && seen_titles.find(normalized_title) != seen_titles.end()) {
            is_duplicate = true;
        }
        
        if (!is_duplicate) {
            seen_urls.insert(normalized_url);
            seen_titles.insert(normalized_title);
            unique_results.push_back(std::move(result));
        }
    }
    
    results = std::move(unique_results);
}

/**
 * @brief Clean snippet text by removing excessive whitespace and truncating
 */
static std::string cleanSnippet(const std::string& text, size_t max_len = 300) {
    std::string cleaned;
    cleaned.reserve(text.size());
    bool prev_space = false;
    
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!prev_space) {
                cleaned += ' ';
                prev_space = true;
            }
        } else {
            cleaned += c;
            prev_space = false;
        }
    }
    
    // Trim leading/trailing spaces
    while (!cleaned.empty() && cleaned.front() == ' ') {
        cleaned.erase(0, 1);
    }
    while (!cleaned.empty() && cleaned.back() == ' ') {
        cleaned.pop_back();
    }
    
    if (cleaned.size() > max_len) {
        cleaned = cleaned.substr(0, max_len);
        // Try to end at a word boundary
        size_t last_space = cleaned.rfind(' ');
        if (last_space > max_len / 2) {
            cleaned = cleaned.substr(0, last_space);
        }
        cleaned += "...";
    }
    
    return cleaned;
}

SearchResponse SearchEngine::searchBaidu(const std::string& query) const {
    SearchResponse response;
    response.query = query;
    response.engine = "baidu";

    std::string encoded = urlEncode(query);
    // Add parameters to get more article-style results
    // tn=news: news search mode (more article-like results)
    // rtt=1: real-time results
    // bsst=1: news search
    std::string url = "https://www.baidu.com/s?wd=" + encoded + "&tn=news&rtt=1&bsst=1";

    std::string html = httpGet(url);
    if (html.empty()) {
        response.success = false;
        response.error_message = "Failed to fetch Baidu search results";
        return response;
    }

    std::vector<SearchResult> results;
    std::set<std::string> seen_urls;  // For deduplication

    // =========================================================================
    // Pattern 1: Standard h3 > a pattern (most common)
    // =========================================================================
    static const std::regex h3_a_re(
        "<h3[^>]*class=\"[^\"]*t[^\"]*\"[^>]*>[\\s\\S]*?<a[^>]*href=\"([^\"]*)\"[^>]*>([\\s\\S]*?)</a>[\\s\\S]*?</h3>",
        std::regex::icase
    );

    auto h3_begin = std::sregex_iterator(html.begin(), html.end(), h3_a_re);
    auto h3_end = std::sregex_iterator();

    for (auto it = h3_begin; it != h3_end && static_cast<int>(results.size()) < max_results_ * 2; ++it) {
        SearchResult result;
        result.url = (*it)[1].str();
        std::string raw_title = (*it)[2].str();
        result.title = decodeHtmlEntities(stripHtmlTags(raw_title));

        // Filter out unwanted results
        if (shouldFilterUrl(result.url) || !isValidTitle(result.title)) {
            continue;
        }

        // Extract snippet from context after the match
        size_t after_pos = it->position() + it->length();
        if (after_pos < html.size()) {
            size_t end_pos = std::min(after_pos + size_t(2000), html.size());
            std::string after_context = html.substr(after_pos, end_pos - after_pos);
            std::string snippet_text = decodeHtmlEntities(stripHtmlTags(after_context));
            result.snippet = cleanSnippet(snippet_text);
        }

        if (!result.title.empty() && !result.url.empty()) {
            results.push_back(std::move(result));
        }
    }

    // =========================================================================
    // Pattern 2: Generic h3 > a pattern (fallback)
    // =========================================================================
    if (results.size() < static_cast<size_t>(max_results_)) {
        static const std::regex h3_generic_re(
            "<h3[^>]*>[\\s\\S]*?<a[^>]*href=\"([^\"]*)\"[^>]*>([\\s\\S]*?)</a>[\\s\\S]*?</h3>",
            std::regex::icase
        );

        auto h3_gen_begin = std::sregex_iterator(html.begin(), html.end(), h3_generic_re);
        auto h3_gen_end = std::sregex_iterator();

        for (auto it = h3_gen_begin; it != h3_gen_end && static_cast<int>(results.size()) < max_results_ * 2; ++it) {
            SearchResult result;
            result.url = (*it)[1].str();
            std::string raw_title = (*it)[2].str();
            result.title = decodeHtmlEntities(stripHtmlTags(raw_title));

            if (shouldFilterUrl(result.url) || !isValidTitle(result.title)) {
                continue;
            }

            size_t after_pos = it->position() + it->length();
            if (after_pos < html.size()) {
                size_t end_pos = std::min(after_pos + size_t(2000), html.size());
                std::string after_context = html.substr(after_pos, end_pos - after_pos);
                std::string snippet_text = decodeHtmlEntities(stripHtmlTags(after_context));
                result.snippet = cleanSnippet(snippet_text);
            }

            if (!result.title.empty() && !result.url.empty()) {
                results.push_back(std::move(result));
            }
        }
    }

    // =========================================================================
    // Pattern 3: c-container class (new Baidu layout)
    // =========================================================================
    if (results.size() < static_cast<size_t>(max_results_)) {
        static const std::regex container_re(
            "<div[^>]*class=\"[^\"]*c-container[^\"]*\"[^>]*>([\\s\\S]*?)</div>\\s*</div>",
            std::regex::icase
        );

        auto container_begin = std::sregex_iterator(html.begin(), html.end(), container_re);
        auto container_end = std::sregex_iterator();

        for (auto it = container_begin; it != container_end && static_cast<int>(results.size()) < max_results_ * 2; ++it) {
            std::string block = (*it)[1].str();
            
            // Extract title and URL from the container
            static const std::regex title_url_re(
                "<a[^>]*href=\"([^\"]*)\"[^>]*>([\\s\\S]*?)</a>",
                std::regex::icase
            );
            
            std::smatch title_match;
            if (std::regex_search(block, title_match, title_url_re)) {
                SearchResult result;
                result.url = title_match[1].str();
                result.title = decodeHtmlEntities(stripHtmlTags(title_match[2].str()));

                if (shouldFilterUrl(result.url) || !isValidTitle(result.title)) {
                    continue;
                }

                // Extract snippet from the container
                static const std::regex snippet_re(
                    "<span[^>]*class=\"[^\"]*content-right[^\"]*\"[^>]*>([\\s\\S]*?)</span>",
                    std::regex::icase
                );
                std::smatch snippet_match;
                if (std::regex_search(block, snippet_match, snippet_re)) {
                    result.snippet = cleanSnippet(decodeHtmlEntities(stripHtmlTags(snippet_match[1].str())));
                } else {
                    // Fallback: get all text after the title
                    result.snippet = cleanSnippet(decodeHtmlEntities(stripHtmlTags(block)));
                }

                if (!result.title.empty() && !result.url.empty()) {
                    results.push_back(std::move(result));
                }
            }
        }
    }

    // =========================================================================
    // Pattern 4: mu= attribute pattern (for news/real-time results)
    // =========================================================================
    if (results.size() < static_cast<size_t>(max_results_)) {
        static const std::regex mu_re(
            "<a[\\s]+[^>]*mu=\"([^\"]*)\"[^>]*>([\\s\\S]*?)</a>",
            std::regex::icase
        );

        auto mu_begin = std::sregex_iterator(html.begin(), html.end(), mu_re);
        auto mu_end = std::sregex_iterator();

        for (auto it = mu_begin; it != mu_end && static_cast<int>(results.size()) < max_results_ * 2; ++it) {
            SearchResult result;
            result.url = (*it)[1].str();
            std::string raw_title = (*it)[2].str();
            result.title = decodeHtmlEntities(stripHtmlTags(raw_title));

            if (shouldFilterUrl(result.url) || !isValidTitle(result.title)) {
                continue;
            }

            size_t after_pos = it->position() + it->length();
            if (after_pos < html.size()) {
                std::string context = html.substr(after_pos, 1000);
                result.snippet = cleanSnippet(decodeHtmlEntities(stripHtmlTags(context)));
            }

            if (!result.title.empty() && !result.url.empty()) {
                results.push_back(std::move(result));
            }
        }
    }

    // =========================================================================
    // Pattern 5: data-link attribute pattern
    // =========================================================================
    if (results.size() < static_cast<size_t>(max_results_)) {
        static const std::regex data_link_re(
            "<a[^>]*data-link=\"([^\"]*)\"[^>]*>([\\s\\S]*?)</a>",
            std::regex::icase
        );

        auto dl_begin = std::sregex_iterator(html.begin(), html.end(), data_link_re);
        auto dl_end = std::sregex_iterator();

        for (auto it = dl_begin; it != dl_end && static_cast<int>(results.size()) < max_results_ * 2; ++it) {
            SearchResult result;
            result.url = (*it)[1].str();
            std::string raw_title = (*it)[2].str();
            result.title = decodeHtmlEntities(stripHtmlTags(raw_title));

            if (shouldFilterUrl(result.url) || !isValidTitle(result.title)) {
                continue;
            }

            size_t after_pos = it->position() + it->length();
            if (after_pos < html.size()) {
                std::string context = html.substr(after_pos, 1000);
                result.snippet = cleanSnippet(decodeHtmlEntities(stripHtmlTags(context)));
            }

            if (!result.title.empty() && !result.url.empty()) {
                results.push_back(std::move(result));
            }
        }
    }

    // =========================================================================
    // Deduplicate and limit results
    // =========================================================================
    deduplicateResults(results);
    
    // Limit to max_results_
    if (results.size() > static_cast<size_t>(max_results_)) {
        results.resize(max_results_);
    }

    response.results = std::move(results);

    if (response.results.empty()) {
        response.success = false;
        response.error_message = "No results found (page structure may have changed)";
    }

    return response;
}

// ============================================================================
// Bing search
// ============================================================================

/**
 * @brief Decode Bing redirect URL to extract actual URL.
 * Bing uses redirect URLs like: https://www.bing.com/ck/a?...&u=a1aHR0cHM6Ly...
 * The 'u' parameter contains base64-like encoded actual URL.
 */
static std::string decodeBingUrl(const std::string& url) {
    // Check if it's a Bing redirect URL
    if (url.find("bing.com/ck/a?") == std::string::npos) {
        return url;  // Not a redirect URL, return as-is
    }
    
    // First decode HTML entities (&amp; -> &)
    std::string decoded_url = url;
    size_t pos = 0;
    while ((pos = decoded_url.find("&amp;", pos)) != std::string::npos) {
        decoded_url.replace(pos, 5, "&");
        pos += 1;
    }
    
    // Find the 'u=' parameter
    size_t u_pos = decoded_url.find("&u=");
    if (u_pos == std::string::npos) {
        return url;
    }
    u_pos += 3;  // Skip "&u="
    
    // Extract the encoded URL
    size_t u_end = decoded_url.find('&', u_pos);
    std::string encoded = (u_end == std::string::npos) 
        ? decoded_url.substr(u_pos) 
        : decoded_url.substr(u_pos, u_end - u_pos);
    
    // The encoded URL starts with a prefix like 'a1' followed by base64
    // Skip the prefix (first 2 characters typically)
    if (encoded.size() > 2) {
        encoded = encoded.substr(2);
    }
    
    // Base64 decode (URL-safe variant: - instead of +, _ instead of /)
    // Convert back to standard base64
    std::string b64 = encoded;
    for (char& c : b64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    
    // Add padding if needed
    while (b64.size() % 4 != 0) {
        b64 += '=';
    }
    
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<unsigned char>(base64_chars[i])] = i;
    
    int val = 0, valb = -8;
    for (unsigned char c : b64) {
        if (c == '=') break;
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return result.empty() ? url : result;
}

SearchResponse SearchEngine::searchBing(const std::string& query) const {
    SearchResponse response;
    response.query = query;
    response.engine = "bing";

    std::string encoded = urlEncode(query);
    // Use English language preference for Bing to get English results
    // Use global.bing.com to avoid region redirect, setlang=en forces English
    std::string url = "https://global.bing.com/search?q=" + encoded + 
                      "&count=" + std::to_string(max_results_) + 
                      "&setlang=en&cc=us&setmkt=en-US";

    std::string html = httpGet(url, "en-US,en;q=0.9");
    if (html.empty()) {
        response.success = false;
        response.error_message = "Failed to fetch Bing search results";
        return response;
    }

    std::vector<SearchResult> results;

    // Bing uses <li class="...b_algo..."> for each result
    static const std::regex b_algo_re(
        "<li[\\s]+class=\"[^\"]*b_algo[^\"]*\"[^>]*>([\\s\\S]*?)</li>",
        std::regex::icase
    );

    auto begin = std::sregex_iterator(html.begin(), html.end(), b_algo_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end && static_cast<int>(results.size()) < max_results_; ++it) {
        std::string block = (*it)[1].str();

        // Extract title and URL from <h2><a href="...">...</a></h2>
        static const std::regex title_re(
            "<h2[^>]*>[\\s]*<a[^>]*href=\"([^\"]*)\"[^>]*>([\\s\\S]*?)</a>",
            std::regex::icase
        );

        std::smatch title_match;
        if (std::regex_search(block, title_match, title_re)) {
            SearchResult result;
            result.url = decodeBingUrl(title_match[1].str());
            result.title = decodeHtmlEntities(stripHtmlTags(title_match[2].str()));

            // Extract snippet from <p> tags
            static const std::regex snippet_re(
                "<p[^>]*>([\\s\\S]*?)</p>",
                std::regex::icase
            );

            auto p_begin = std::sregex_iterator(block.begin(), block.end(), snippet_re);
            auto p_end = std::sregex_iterator();

            std::string combined_snippet;
            for (auto pit = p_begin; pit != p_end; ++pit) {
                std::string p_text = decodeHtmlEntities(stripHtmlTags((*pit)[1].str()));
                if (!p_text.empty()) {
                    if (!combined_snippet.empty()) combined_snippet += " ";
                    combined_snippet += p_text;
                }
            }

            // Clean whitespace
            combined_snippet.erase(std::unique(combined_snippet.begin(), combined_snippet.end(),
                [](char a, char b) { return std::isspace(a) && std::isspace(b); }),
                combined_snippet.end());
            if (!combined_snippet.empty() && combined_snippet[0] == ' ')
                combined_snippet.erase(0, 1);

            result.snippet = combined_snippet;

            if (!result.title.empty()) {
                results.push_back(std::move(result));
            }
        }
    }

    response.results = std::move(results);

    if (response.results.empty()) {
        response.success = false;
        response.error_message = "No results found (page structure may have changed)";
    }

    return response;
}

// ============================================================================
// Bilibili search
// ============================================================================

/**
 * @brief Clean Bilibili title by removing <em class="keyword"> tags.
 * Bilibili API returns titles with keyword highlighting like:
 * "【<em class=\"keyword\">Test</em>-Kitchen】RTX 2070..."
 */
static std::string cleanBilibiliTitle(const std::string& title) {
    std::string result = title;
    
    // Remove <em class="keyword"> and </em> tags
    static const std::regex em_tag_re("<em[^>]*>|</em>", std::regex::icase);
    result = std::regex_replace(result, em_tag_re, "");
    
    // Decode HTML entities
    static const std::pair<std::string, std::string> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&#39;", "'"}, {"&apos;", "'"}, {"&nbsp;", " "},
        {"&#x27;", "'"}
    };
    
    for (const auto& [encoded, decoded] : entities) {
        size_t pos = 0;
        while ((pos = result.find(encoded, pos)) != std::string::npos) {
            result.replace(pos, encoded.length(), decoded);
            pos += decoded.length();
        }
    }
    
    return result;
}

/**
 * @brief Format play count for display (e.g., 40890 -> "4万" or "40890")
 */
static std::string formatPlayCount(int64_t play) {
    if (play >= 100000000) {
        return std::to_string(play / 100000000) + "亿";
    } else if (play >= 10000) {
        return std::to_string(play / 10000) + "万";
    }
    return std::to_string(play);
}

SearchResponse SearchEngine::searchBilibili(const std::string& query) const {
    SearchResponse response;
    response.query = query;
    response.engine = "bilibili";

    std::string encoded = urlEncode(query);
    
    // Use Bilibili's official search API
    // API endpoint: https://api.bilibili.com/x/web-interface/search/type
    // Parameters: search_type=video, keyword=xxx
    std::string url = "https://api.bilibili.com/x/web-interface/search/type?search_type=video&keyword=" + encoded;

    // Need special headers for Bilibili API
    HttpResponse http_response;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.success = false;
        response.error_message = "Failed to initialize libcurl";
        return response;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("User-Agent: " + user_agent_).c_str());
    headers = curl_slist_append(headers, "Accept: application/json, text/plain, */*");
    headers = curl_slist_append(headers, "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
    headers = curl_slist_append(headers, "Referer: https://www.bilibili.com");
    headers = curl_slist_append(headers, "Origin: https://www.bilibili.com");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &http_response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &http_response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds_));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(timeout_seconds_));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        response.success = false;
        response.error_message = "HTTP request failed: " + std::string(curl_easy_strerror(res));
        return response;
    }

    if (http_response.body.empty()) {
        response.success = false;
        response.error_message = "Empty response from Bilibili API";
        return response;
    }

    std::vector<SearchResult> results;

    try {
        auto json = nlohmann::json::parse(http_response.body);
        
        // Check API response code
        int code = json.value("code", -1);
        if (code != 0) {
            response.success = false;
            response.error_message = "Bilibili API error: " + json.value("message", "Unknown error");
            return response;
        }

        // Parse search results from data.result array
        if (json.contains("data") && json["data"].contains("result") && json["data"]["result"].is_array()) {
            for (const auto& item : json["data"]["result"]) {
                if (static_cast<int>(results.size()) >= max_results_) break;

                // Skip non-video results (e.g., "ketang" type for courses)
                std::string type = item.value("type", "");
                if (type != "video" && type != "media_bangumi" && type != "media_ft") {
                    continue;
                }

                SearchResult result;
                
                // Title (with keyword highlighting tags)
                result.title = cleanBilibiliTitle(item.value("title", ""));
                
                // URL - use arcurl or construct from bvid
                std::string arcurl = item.value("arcurl", "");
                std::string bvid = item.value("bvid", "");
                if (!arcurl.empty()) {
                    if (arcurl.find("http") == 0) {
                        result.url = arcurl;
                    } else {
                        result.url = "https:" + arcurl;
                    }
                } else if (!bvid.empty()) {
                    result.url = "https://www.bilibili.com/video/" + bvid;
                }
                
                // Description/snippet
                result.snippet = item.value("description", "");
                
                // Extra info: author, play count, duration
                std::string author = item.value("author", "");
                int64_t play = item.value("play", 0);
                std::string duration = item.value("duration", "");
                
                if (!author.empty() || play > 0 || !duration.empty()) {
                    result.extra = "UP: " + author;
                    if (play > 0) {
                        result.extra += " | Play: " + formatPlayCount(play);
                    }
                    if (!duration.empty()) {
                        result.extra += " | Duration: " + duration;
                    }
                }

                if (!result.title.empty() && !result.url.empty()) {
                    results.push_back(std::move(result));
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[SearchEngine] Failed to parse Bilibili API response: " << e.what() << "\n";
        response.success = false;
        response.error_message = "Failed to parse API response: " + std::string(e.what());
        return response;
    }

    response.results = std::move(results);

    if (response.results.empty()) {
        response.success = false;
        response.error_message = "No results found";
    }

    return response;
}

// ============================================================================
// Multi-engine search (parallel)
// ============================================================================

std::vector<SearchResponse> SearchEngine::searchMulti(
    const std::string& query,
    const std::vector<std::string>& engines) const
{
    std::vector<std::future<SearchResponse>> futures;

    for (const auto& engine : engines) {
        futures.push_back(std::async(std::launch::async, [this, &query, &engine]() {
            if (engine == "baidu") {
                return searchBaidu(query);
            } else if (engine == "bing") {
                return searchBing(query);
            } else if (engine == "bilibili") {
                return searchBilibili(query);
            } else {
                SearchResponse resp;
                resp.query = query;
                resp.engine = engine;
                resp.success = false;
                resp.error_message = "Unknown engine: " + engine;
                return resp;
            }
        }));
    }

    std::vector<SearchResponse> responses;
    responses.reserve(futures.size());

    for (auto& f : futures) {
        try {
            responses.push_back(f.get());
        } catch (const std::exception& e) {
            SearchResponse resp;
            resp.success = false;
            resp.error_message = "Exception: " + std::string(e.what());
            responses.push_back(std::move(resp));
        }
    }

    return responses;
}

} // namespace mcpp
