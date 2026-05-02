# search++ v1 (search-plus-plus-v1-mcp)

[![Version](https://img.shields.io/badge/version-v1.0.0-blue.svg)](https://github.com/yourusername/search-plus-plus-v1-mcp)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-orange.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/license-GPL%203.0-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-Release%20O3%2BLTO-brightgreen.svg)]()

> 🚀 **The first C++ high-performance MCP search engine with Baidu & Bilibili support**

A high-performance MCP (Model Context Protocol) server implementation in C++17, providing multi-engine web search capabilities for AI assistants.

## Why search++?

### 🔥 Advantages Over Other MCP Search Servers

| Feature | **search++** | MCP MixSearch | Open Web Search | SerpApi MCP |
|---------|--------------|--------------|-----------------|-------------|
| **Language** | C++17 ⚡ | Python | Python | Python |
| **Performance** | O3+LTO ⚡⚡⚡ | 🐢 | 🐢 | 🐢 |
| **Baidu** | ✅ **Exclusive** | ❌ | ✅ | ✅ |
| **Bilibili** | ✅ **Exclusive** | ❌ | ❌ | ❌ |
| **Single Binary** | ✅ No deps | ❌ Needs Python | ❌ Needs Python | ❌ Needs Python |
| **Free** | ✅ Completely Free | ✅ | ✅ | ❌ Needs API Key |
| **Chinese Support** | ✅ Native | ❌ | ⚠️ Limited | ⚠️ Limited |

### 🎯 Why Choose search++?

1. **Extreme Performance**
   - C++17 + O3 optimization + LTO
   - Zero-copy I/O design
   - Single binary deployment, no Python environment needed

2. **China-First Design**
   - First MCP with **Baidu search** support
   - First MCP with **Bilibili video search** support
   - Full Chinese search result support

3. **Zero Configuration**
   - Static linking, single binary
   - No Python environment required
   - No API key needed

4. **Dual Protocol Compatibility**
   - Standard MCP (Content-Length framed)
   - Trae CN compatible (Raw JSON)
   - Auto-detect client format

## Features

### Multi-Engine Search Support

| Engine | Description | Return Data |
|--------|-------------|-------------|
| Baidu | Chinese search engine | Title, URL, Snippet |
| Bing | Microsoft search engine | Title, URL, Snippet |
| Bilibili | Video platform search | Title, URL, View count, Uploader |

### Performance Highlights

- **C++17 Native Implementation**: Zero-overhead abstractions, compile-time optimizations
- **Zero-Copy I/O**: Minimizes memory allocation and data copying
- **Static Linking**: Single binary deployment, no runtime dependencies
- **Buffered I/O**: 4KB read buffer for optimal throughput
- **LTO Enabled**: Link-time optimization for maximum performance

### Protocol Compatibility

- **Standard MCP**: Content-Length framed messages (`Content-Length: N\r\n\r\n{json}`)
- **Trae CN Compatible**: Raw JSON mode for seamless integration
- **Auto-Detection**: Automatically detects and adapts to client format

## Installation

### Prerequisites

- CMake 3.21+
- C++17 compatible compiler (GCC 9+, Clang 10+)
- libcurl development libraries
- nlohmann_json

#### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev
```

#### Fedora

```bash
sudo dnf install gcc-c++ cmake libcurl-devel nlohmann-json-devel
```

#### macOS

```bash
brew install cmake curl nlohmann-json
```

### Build from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/search-plus-plus-v1-mcp.git
cd search-plus-plus-v1-mcp

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# The binary will be at: build/mcpp_example
```

## Configuration

### For Claude Desktop

Add to your Claude Desktop configuration (`~/Library/Application Support/Claude/claude_desktop_config.json` on macOS or `%APPDATA%\Claude\claue_desktop_config.json` on Windows):

```json
{
  "mcpServers": {
    "search-plus-plus-v1-mcp": {
      "command": "/path/to/search-plus-plus-v1-mcp/build/mcpp_example"
    }
  }
}
```

### For Trae CN

Create or edit `.trae/mcp.json` in your project:

```json
{
  "mcpServers": {
    "search-plus-plus-v1-mcp": {
      "command": "bash",
      "args": ["/path/to/search-plus-plus-v1-mcp/mcpp_launcher.sh"],
      "env": {
        "MCPP_BINARY": "/path/to/search++/build/mcpp_example",
        "MCPP_PROJECT_DIR": "/path/to/search++"
      }
    }
  }
}
```

### Using the Launcher Script

The included `mcpp_launcher.sh` handles:
- Library path configuration
- Working directory setup
- Signal forwarding
- Environment variable management

## Available Tools

### search_baidu

Search on Baidu.

```json
{
  "name": "search_baidu",
  "arguments": {
    "query": "your search query",
    "limit": 10
  }
}
```

### search_bing

Search on Bing.

```json
{
  "name": "search_bing",
  "arguments": {
    "query": "your search query",
    "limit": 10
  }
}
```

### search_bilibili

Search for videos on Bilibili.

```json
{
  "name": "search_bilibili",
  "arguments": {
    "query": "your search query",
    "limit": 10
  }
}
```

### search_multi

Search across multiple engines in parallel.

```json
{
  "name": "search_multi",
  "arguments": {
    "query": "your search query",
    "engines": ["baidu", "bing", "bilibili"],
    "limit": 5
  }
}
```

### Additional Tools

| Tool | Description |
|------|-------------|
| `calculator` | Basic arithmetic operations (add, subtract, multiply, divide) |
| `string_util` | String manipulation (upper, lower, reverse, length) |
| `get_time` | Get current date and time with milliseconds |

## Response Format

### Search Result

```json
{
  "query": "search query",
  "engine": "baidu",
  "results": [
    {
      "title": "Result Title",
      "url": "https://example.com",
      "snippet": "Result description...",
      "extra": "Additional info (e.g., view count for Bilibili)"
    }
  ],
  "success": true
}
```

## Architecture

```
mcpp/
├── include/mcpp/           # Header files
│   ├── Server.h            # MCP server implementation
│   ├── Transport.h         # stdio transport layer
│   ├── JsonRpc.h           # JSON-RPC 2.0 dispatcher
│   ├── Tool.h              # Tool registry
│   ├── Resource.h          # Resource registry
│   ├── Prompt.h            # Prompt registry
│   ├── SearchEngine.h      # Multi-engine search
│   └── Types.hpp           # Type definitions
├── src/                    # Implementation files
├── examples/               # Example server
│   └── main.cpp
└── CMakeLists.txt
```

## Development

### Debug Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run Tests

```bash
# Interactive test
./build/mcpp_example

# Send a test request
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0.0"}}}' | ./build/mcpp_example
```

## Technical Details

### Transport Layer

The transport layer supports two message formats:

1. **Content-Length Framed** (Standard MCP):
   ```
   Content-Length: 123\r\n
   \r\n
   {"jsonrpc":"2.0",...}
   ```

2. **Raw JSON** (Trae CN Compatibility):
   ```
   {"jsonrpc":"2.0",...}
   ```

The format is automatically detected from the first incoming message, and responses use the same format.

### Performance Optimizations

- **Zero-copy JSON parsing**: Direct string views where possible
- **Buffered I/O**: 4KB read buffer reduces system calls
- **Move semantics**: Extensive use of std::move for efficiency
- **Static linking**: No dynamic library loading overhead
- **LTO**: Cross-module optimization at link time

## License

GNU General Public License v3.0 - see [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Acknowledgments

- [MCP Specification](https://modelcontextprotocol.io/) - Model Context Protocol
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library for C++
- [libcurl](https://curl.se/libcurl/) - Client-side URL transfer library
