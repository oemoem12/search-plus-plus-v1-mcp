# MCP Server Debugging Guide - Trae CN

## Issue: MCP Server Stuck in "Preparing..." Status

### Root Cause Analysis

Trae CN uses Node.js's `child_process.spawn()` to communicate with MCP servers via stdio transport protocol.
C++ binaries exchange JSON-RPC messages through stdin/stdout using **Content-Length framing format**.

The "Preparing..." status is typically caused by:

| Cause | Description | Solution |
|-------|-------------|----------|
| **stdio pipe buffering** | glibc uses block buffering for stdout in non-TTY environment | Use launcher script to set environment variable |
| **LD_LIBRARY_PATH missing** | spawn() doesn't inherit full LD_LIBRARY_PATH | Set in mcp.json env section |
| **Working directory mismatch** | Binary depends on relative paths | cd to correct directory before launch |
| **Signal not forwarded** | Intermediate shell swallows SIGTERM | Use `exec` to replace shell process |

### File Overview

#### 1. `mcpp_launcher.sh` - Launch Wrapper Script

This script solves the common "preparing" issue when integrating C++ MCP servers with Trae CN.

Core functions:
1. Set LD_LIBRARY_PATH (includes build directory and conda environment)
2. Switch to correct working directory
3. Use exec to replace shell process (ensures proper signal forwarding)
4. Verify binary exists and is executable

#### 2. `mcp.json` - Trae CN MCP Configuration

Place the config file in project root or `.trae/` directory.

### Deployment Steps

#### Step 1: Ensure Binary is Compiled

```bash
cd /path/to/project
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Step 2: Set Launcher Script Permissions

```bash
chmod +x mcpp_launcher.sh
```

#### Step 3: Configure mcp.json

Place `mcp.json` in one of these locations:
- Project root: `/home/cat/Documents/trae_projects/trae/mcp.json`
- Trae config dir: `~/.trae/mcp.json`

#### Step 4: Restart Trae CN

Close and reopen Trae CN. The MCP server should connect properly.

### Debugging Methods

#### Method 1: Test Binary Manually

```bash
# Send an initialize request to test
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0.0"}}}' | ./build/mcpp_example
```

#### Method 2: Test Through Launcher Script

```bash
MCPP_BINARY=/path/to/mcpp_example MCPP_PROJECT_DIR=/path/to/project ./mcpp_launcher.sh
```

#### Method 3: Check Environment Variables

```bash
# View launcher environment variable settings
env | grep MCPP
```

#### Method 4: Use strace for Diagnostics

```bash
# Trace system calls, check stdio read/write
strace -e read,write -f -p $(pgrep mcpp_example)
```

### mcp.json Configuration Options

```json
{
  "mcpServers": {
    "search-plus-plus-v1-mcp": {
      "command": "bash",                           // Use bash to launch script
      "args": ["/absolute/path/to/mcpp_launcher.sh"], // Absolute path to script
      "env": {
        "MCPP_BINARY": "/path/to/mcpp_example",    // Binary file path
        "MCPP_PROJECT_DIR": "/path/to/project"     // Project directory
      },
      "disabled": false                            // Whether to disable this server
    }
  }
}
```

### FAQ

#### Q: Still showing "Preparing..."

A: Check in this order:
1. Does binary exist and is executable: `ls -la /path/to/mcpp_example`
2. Are dependencies complete: `ldd /path/to/mcpp_example`
3. Manual test: `echo '{}' | /path/to/mcpp_example`
4. Check Trae CN logs

#### Q: ldd shows missing libraries

A: Install missing dependencies:
```bash
sudo apt install libcurl4-openssl-dev nlohmann-json3-dev
```

#### Q: How to add more MCP servers

A: Add more entries in `mcp.json`:
```json
{
  "mcpServers": {
    "server1": { ... },
    "server2": { ... }
  }
}
```

### Protocol Details

This MCP server uses **Content-Length framing format**:

```
Content-Length: 123\r\n
\r\n
{"jsonrpc":"2.0",...}
```

Key points:
- Each message has a `Content-Length` header
- Header followed by two `\r\n` (empty line)
- Then N bytes of JSON-RPC message body
- Server reads from stdin, writes to stdout, and calls `fflush(stdout)`

### Requirements

- **OS**: Linux x86_64
- **Compiler**: GCC/Clang with C++17 support
- **Dependencies**:
  - libcurl
  - nlohmann_json
- **Node.js**: Built-in to Trae CN
