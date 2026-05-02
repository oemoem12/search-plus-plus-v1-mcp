#ifndef MCPP_TRANSPORT_H
#define MCPP_TRANSPORT_H

#include <mcpp/Types.hpp>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <queue>

namespace mcpp {

/**
 * @brief High-performance stdio-based transport for MCP protocol.
 *
 * Supports two message formats:
 * 1. Content-Length framed (standard MCP): "Content-Length: N\r\n\r\n{json}"
 * 2. Raw JSON (Trae CN compatibility): "{json}"
 *
 * Automatically detects the format of incoming messages and responds in kind.
 * Thread-safe message queue for outgoing messages.
 * Buffered I/O for optimal performance.
 */
class Transport {
public:
    Transport();
    ~Transport();

    /// Non-copyable
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    /// Movable
    Transport(Transport&& other) noexcept;
    Transport& operator=(Transport&& other) noexcept;

    /**
     * @brief Read the next message from stdin.
     * @param timeout_ms Maximum wait time in milliseconds (0 = blocking).
     * @return Parsed JSON message, or empty optional on EOF/error.
     */
    [[nodiscard]] std::optional<json> readMessage(int timeout_ms = 0);

    /**
     * @brief Write a message to stdout.
     * 
     * Automatically uses the same format as the incoming message:
     * - If incoming was raw JSON, response is raw JSON
     * - If incoming was Content-Length framed, response is framed
     * 
     * @param message The JSON message to send.
     * @return true on success, false on write error.
     */
    [[nodiscard]] bool writeMessage(const json& message);

    /**
     * @brief Queue a message for sending (thread-safe).
     * @param message The JSON message to enqueue.
     */
    bool enqueueMessage(json message);

    /**
     * @brief Flush all queued messages to stdout.
     * @return true on success, false on error.
     */
    bool flushQueue();

    /**
     * @brief Get the raw string from the last read operation (for debugging).
     */
    [[nodiscard]] std::string getLastRaw() const;

    /**
     * @brief Get total bytes read for diagnostics.
     */
    [[nodiscard]] uint64_t getBytesRead() const;

    /**
     * @brief Get total bytes written for diagnostics.
     */
    [[nodiscard]] uint64_t getBytesWritten() const;

    /**
     * @brief Check if transport has reached EOF.
     */
    [[nodiscard]] bool isEof() const;

    /**
     * @brief Check if transport is still connected (stdin not closed).
     */
    [[nodiscard]] bool isConnected() const noexcept { return !eof_.load(std::memory_order_relaxed); }

    /**
     * @brief Set a callback for when a message is received.
     */
    void setOnReceive(std::function<void(json)> callback);

private:
    /// Read raw JSON message (no Content-Length header)
    [[nodiscard]] std::optional<json> readRawJson();

    /// Read Content-Length framed message
    [[nodiscard]] std::optional<json> readContentLengthFramed();

    // Buffered read state
    static constexpr size_t kReadBufferSize = 4096;
    std::vector<char> read_buffer_;
    size_t read_buffer_pos_ = 0;
    size_t read_buffer_len_ = 0;

    // Message parsing state
    enum class ParseState { Header, Body };
    ParseState parse_state_ = ParseState::Header;
    size_t content_length_ = 0;
    std::string header_buffer_;
    std::string body_buffer_;

    // Last raw message (for debugging)
    std::string last_raw_;

    // Outgoing message queue (thread-safe)
    std::mutex queue_mutex_;
    std::queue<json> message_queue_;

    // Statistics
    std::atomic<uint64_t> bytes_read_{0};
    std::atomic<uint64_t> bytes_written_{0};
    std::atomic<bool> eof_{false};

    // Receive callback
    std::function<void(json)> on_receive_;

    // Output format mode: true = raw JSON, false = Content-Length framed
    // Automatically set based on first incoming message format
    bool raw_json_mode_{false};
};

} // namespace mcpp

#endif // MCPP_TRANSPORT_H
