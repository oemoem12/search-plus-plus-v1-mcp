# search++ v1.0.0 Release

## Download

| Platform | Architecture | File | Size |
|----------|-------------|------|------|
| Linux | x86_64 | [search-plus-plus-v1-mcp-linux-x64.tar.gz]() | ~650KB |

## Requirements

- Linux x86_64
- libcurl (usually pre-installed)

### Install libcurl (if not available)

```bash
# Ubuntu/Debian
sudo apt install libcurl4

# Fedora
sudo dnf install libcurl

# Arch Linux
sudo pacman -S curl
```

## Quick Start

```bash
# Extract
tar -xzf search-plus-plus-v1-mcp-linux-x64.tar.gz
cd search-plus-plus-v1-mcp-linux-x64

# Make executable
chmod +x mcpp_example

# Test
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0.0"}}}' | ./mcpp_example
```

## Configuration

### Claude Desktop

Add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "search-plus-plus-v1-mcp": {
      "command": "/path/to/search-plus-plus-v1-mcp-linux-x64/mcpp_example"
    }
  }
}
```

### Trae CN

Add to `.trae/mcp.json`:

```json
{
  "mcpServers": {
    "search-plus-plus-v1-mcp": {
      "command": "bash",
      "args": ["/path/to/search-plus-plus-v1-mcp-linux-x64/mcpp_launcher.sh"],
      "env": {
        "MCPP_BINARY": "/path/to/search-plus-plus-v1-mcp-linux-x64/mcpp_example"
      }
    }
  }
}
```

## License

GPL 3.0 - See LICENSE file
