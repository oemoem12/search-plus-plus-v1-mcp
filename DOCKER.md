# Docker Images

Docker images are available on [Docker Hub](https://hub.docker.com/r/oemoem12/search-plus-plus-mcp).

## Available Tags

| Tag | Description |
|-----|-------------|
| `latest` | Latest stable release |
| `1.0.0` | Specific version |
| `1.0` | Minor version |

## Usage

### Basic Usage

```bash
docker run --rm oemoem12/search-plus-plus-mcp:latest
```

### With MCP Client

```bash
docker run --rm -i oemoem12/search-plus-plus-mcp:latest
```

### Claude Desktop Configuration

Add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "search-plus-plus-mcp": {
      "command": "docker",
      "args": ["run", "--rm", "-i", "oemoem12/search-plus-plus-mcp:latest"]
    }
  }
}
```

### Trae Configuration

Add to `.trae/mcp.json`:

```json
{
  "mcpServers": {
    "search-plus-plus-mcp": {
      "command": "docker",
      "args": ["run", "--rm", "-i", "oemoem12/search-plus-plus-mcp:latest"]
    }
  }
}
```

## Build Locally

```bash
docker build -t search-plus-plus-mcp .
```
