#include <mcpp/Transport.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <system_error>

namespace mcpp {

Transport::Transport()
    : read_buffer_(kReadBufferSize)
{
}

Transport::~Transport() = default;

Transport::Transport(Transport&& other) noexcept
    : read_buffer_(std::move(other.read_buffer_)),
      read_buffer_pos_(other.read_buffer_pos_),
      read_buffer_len_(other.read_buffer_len_),
      parse_state_(other.parse_state_),
      content_length_(other.content_length_),
      header_buffer_(std::move(other.header_buffer_)),
      body_buffer_(std::move(other.body_buffer_)),
      last_raw_(std::move(other.last_raw_)),
      message_queue_(std::move(other.message_queue_)),
      bytes_read_(other.bytes_read_.load()),
      bytes_written_(other.bytes_written_.load()),
      eof_(other.eof_.load()),
      on_receive_(std::move(other.on_receive_)),
      raw_json_mode_(other.raw_json_mode_)
{
}

Transport& Transport::operator=(Transport&& other) noexcept
{
    if (this != &other) {
        read_buffer_ = std::move(other.read_buffer_);
        read_buffer_pos_ = other.read_buffer_pos_;
        read_buffer_len_ = other.read_buffer_len_;
        parse_state_ = other.parse_state_;
        content_length_ = other.content_length_;
        header_buffer_ = std::move(other.header_buffer_);
        body_buffer_ = std::move(other.body_buffer_);
        last_raw_ = std::move(other.last_raw_);
        message_queue_ = std::move(other.message_queue_);
        bytes_read_.store(other.bytes_read_.load());
        bytes_written_.store(other.bytes_written_.load());
        eof_.store(other.eof_.load());
        on_receive_ = std::move(other.on_receive_);
        raw_json_mode_ = other.raw_json_mode_;
    }
    return *this;
}

void Transport::setOnReceive(std::function<void(json)> callback) {
    on_receive_ = std::move(callback);
}

bool Transport::enqueueMessage(json message) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    message_queue_.push(std::move(message));
    return true;
}

std::optional<json> Transport::readMessage(int timeout_ms) {
    if (eof_.load(std::memory_order_relaxed)) {
        return std::nullopt;
    }

    // Check if stdin has data available
    if (timeout_ms != 0) {
        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        pfd.revents = 0;

        const int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0 || ret == 0) {
            return std::nullopt;
        }
    }

    // Fill the read buffer if empty
    if (read_buffer_pos_ >= read_buffer_len_) {
        const ssize_t n = read(STDIN_FILENO, read_buffer_.data(), kReadBufferSize);
        if (n <= 0) {
            if (n == 0) {
                eof_.store(true, std::memory_order_relaxed);
            }
            return std::nullopt;
        }
        read_buffer_len_ = static_cast<size_t>(n);
        read_buffer_pos_ = 0;
        bytes_read_.fetch_add(n, std::memory_order_relaxed);
        fprintf(stderr, "[MCPP] Received %zd bytes\n", n);
    }

    // Detect message format: Content-Length framed or raw JSON
    // Skip leading whitespace/newlines
    while (read_buffer_pos_ < read_buffer_len_) {
        char c = read_buffer_[read_buffer_pos_];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++read_buffer_pos_;
        } else {
            break;
        }
    }

    // Need more data?
    if (read_buffer_pos_ >= read_buffer_len_) {
        return std::nullopt;
    }

    char first_char = read_buffer_[read_buffer_pos_];

    fprintf(stderr, "[MCPP] First char: '%c' (0x%02x)\n", first_char, (unsigned char)first_char);

    // Check if it starts with '{' - raw JSON mode
    if (first_char == '{') {
        fprintf(stderr, "[MCPP] Parsing as raw JSON\n");
        raw_json_mode_ = true; // Remember to respond in raw JSON format
        return readRawJson();
    }

    // Otherwise, assume Content-Length framed mode
    return readContentLengthFramed();
}

std::optional<json> Transport::readRawJson() {
    fprintf(stderr, "[MCPP] readRawJson called, buffer len=%zu, pos=%zu\n", read_buffer_len_, read_buffer_pos_);
    
    // Find the end of JSON by counting braces
    int brace_count = 0;
    bool in_string = false;
    bool escape_next = false;
    size_t start_pos = read_buffer_pos_;

    while (true) {
        // Need more data?
        if (read_buffer_pos_ >= read_buffer_len_) {
            fprintf(stderr, "[MCPP] Need more data, reading...\n");
            // Shift remaining data to beginning
            if (start_pos > 0) {
                size_t remaining = read_buffer_len_ - start_pos;
                if (remaining > 0) {
                    std::memmove(read_buffer_.data(), read_buffer_.data() + start_pos, remaining);
                }
                read_buffer_pos_ = remaining;
                read_buffer_len_ = remaining;
                start_pos = 0;
            } else {
                read_buffer_pos_ = 0;
                read_buffer_len_ = 0;
            }

            const ssize_t n = read(STDIN_FILENO, read_buffer_.data() + read_buffer_len_, 
                                   kReadBufferSize - read_buffer_len_);
            if (n <= 0) {
                if (n == 0) eof_.store(true, std::memory_order_relaxed);
                fprintf(stderr, "[MCPP] read returned %zd, eof=%d\n", n, eof_.load());
                return std::nullopt;
            }
            read_buffer_len_ += static_cast<size_t>(n);
            bytes_read_.fetch_add(n, std::memory_order_relaxed);
            fprintf(stderr, "[MCPP] Read %zd more bytes, total buffer=%zu\n", n, read_buffer_len_);
        }

        char c = read_buffer_[read_buffer_pos_++];

        if (escape_next) {
            escape_next = false;
            continue;
        }

        if (in_string) {
            if (c == '\\') {
                escape_next = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
            } else if (c == '{') {
                ++brace_count;
            } else if (c == '}') {
                --brace_count;
                fprintf(stderr, "[MCPP] brace_count=%d\n", brace_count);
                if (brace_count == 0) {
                    // Found complete JSON
                    std::string json_str(read_buffer_.data() + start_pos, read_buffer_pos_ - start_pos);
                    fprintf(stderr, "[MCPP] Found complete JSON: %s\n", json_str.c_str());
                    try {
                        last_raw_ = json_str;
                        return json::parse(json_str);
                    } catch (const json::parse_error& e) {
                        fprintf(stderr, "[MCPP] JSON parse error: %s\n", e.what());
                        return std::nullopt;
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<json> Transport::readContentLengthFramed() {
    // Parse header to get Content-Length
    while (parse_state_ != ParseState::Body) {
        if (read_buffer_pos_ >= read_buffer_len_) {
            const ssize_t n = read(STDIN_FILENO, read_buffer_.data(), kReadBufferSize);
            if (n <= 0) {
                if (n == 0) eof_.store(true, std::memory_order_relaxed);
                return std::nullopt;
            }
            read_buffer_len_ = static_cast<size_t>(n);
            read_buffer_pos_ = 0;
            bytes_read_.fetch_add(n, std::memory_order_relaxed);
        }

        // Parse Content-Length header
        if (header_buffer_.empty()) {
            static const char kHeaderPrefix[] = "Content-Length: ";
            static constexpr size_t kPrefixLen = sizeof(kHeaderPrefix) - 1;

            if (read_buffer_len_ - read_buffer_pos_ < kPrefixLen) {
                header_buffer_.append(read_buffer_.data() + read_buffer_pos_,
                                     read_buffer_len_ - read_buffer_pos_);
                read_buffer_pos_ = read_buffer_len_;
                continue;
            }

            if (std::memcmp(read_buffer_.data() + read_buffer_pos_, kHeaderPrefix, kPrefixLen) != 0) {
                ++read_buffer_pos_;
                header_buffer_.clear();
                continue;
            }

            read_buffer_pos_ += kPrefixLen;

            std::string len_str;
            while (read_buffer_pos_ < read_buffer_len_) {
                const char c = read_buffer_[read_buffer_pos_];
                if (c >= '0' && c <= '9') {
                    len_str += c;
                    ++read_buffer_pos_;
                } else {
                    break;
                }
            }

            if (!len_str.empty()) {
                content_length_ = std::stoull(len_str);
            }

            // Skip \r\n\r\n
            while (read_buffer_pos_ < read_buffer_len_) {
                if (read_buffer_[read_buffer_pos_] == '\r' &&
                    read_buffer_pos_ + 3 < read_buffer_len_ &&
                    read_buffer_[read_buffer_pos_ + 1] == '\n' &&
                    read_buffer_[read_buffer_pos_ + 2] == '\r' &&
                    read_buffer_[read_buffer_pos_ + 3] == '\n') {
                    read_buffer_pos_ += 4;
                    break;
                }
                ++read_buffer_pos_;
            }

            parse_state_ = ParseState::Body;
        } else {
            header_buffer_.clear();
            parse_state_ = ParseState::Header;
        }
    }

    // Read body
    if (body_buffer_.size() < content_length_) {
        body_buffer_.resize(content_length_);
    }

    size_t body_read = 0;
    while (body_read < content_length_) {
        if (read_buffer_pos_ >= read_buffer_len_) {
            const ssize_t n = read(STDIN_FILENO, read_buffer_.data(), kReadBufferSize);
            if (n <= 0) {
                if (n == 0) eof_.store(true, std::memory_order_relaxed);
                return std::nullopt;
            }
            read_buffer_len_ = static_cast<size_t>(n);
            read_buffer_pos_ = 0;
            bytes_read_.fetch_add(n, std::memory_order_relaxed);
        }

        size_t to_copy = std::min(content_length_ - body_read, read_buffer_len_ - read_buffer_pos_);
        std::memcpy(body_buffer_.data() + body_read, read_buffer_.data() + read_buffer_pos_, to_copy);
        body_read += to_copy;
        read_buffer_pos_ += to_copy;
    }

    // Parse JSON
    std::string body_str(body_buffer_.begin(), body_buffer_.begin() + content_length_);
    last_raw_ = body_str;

    // Reset state for next message
    parse_state_ = ParseState::Header;
    header_buffer_.clear();
    content_length_ = 0;
    body_buffer_.clear();

    try {
        return json::parse(body_str);
    } catch (const json::parse_error& e) {
        return std::nullopt;
    }
}

bool Transport::writeMessage(const json& message) {
    const std::string body = message.dump();

    if (raw_json_mode_) {
        // Raw JSON mode (Trae CN compatibility): just send JSON with newline
        fprintf(stderr, "[MCPP] Writing in raw JSON mode\n");
        if (write(STDOUT_FILENO, body.data(), body.size()) != static_cast<ssize_t>(body.size())) {
            return false;
        }
        // Add newline to separate messages
        if (write(STDOUT_FILENO, "\n", 1) != 1) {
            return false;
        }
        fflush(stdout);
        bytes_written_.fetch_add(body.size() + 1, std::memory_order_relaxed);
        return true;
    }

    // Content-Length framed mode (standard MCP)
    const std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";

    // Write header
    if (write(STDOUT_FILENO, header.data(), header.size()) != static_cast<ssize_t>(header.size())) {
        return false;
    }

    // Write body
    if (write(STDOUT_FILENO, body.data(), body.size()) != static_cast<ssize_t>(body.size())) {
        return false;
    }

    // Flush
    fflush(stdout);

    bytes_written_.fetch_add(header.size() + body.size(), std::memory_order_relaxed);
    return true;
}

bool Transport::flushQueue() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!message_queue_.empty()) {
        if (!writeMessage(message_queue_.front())) {
            return false;
        }
        message_queue_.pop();
    }
    return true;
}

std::string Transport::getLastRaw() const {
    return last_raw_;
}

uint64_t Transport::getBytesRead() const {
    return bytes_read_.load(std::memory_order_relaxed);
}

uint64_t Transport::getBytesWritten() const {
    return bytes_written_.load(std::memory_order_relaxed);
}

bool Transport::isEof() const {
    return eof_.load(std::memory_order_relaxed);
}

} // namespace mcpp
