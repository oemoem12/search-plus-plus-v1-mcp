#!/bin/bash
# =============================================================================
# mcpp_launcher.sh - MCP Server Launcher for Trae CN
# =============================================================================
# This wrapper script solves the common "preparing" issue when
# Trae CN's Node.js MCP client spawns a C++ binary via child_process.spawn().
#
# Root causes this script addresses:
#   1. LD_LIBRARY_PATH not inherited by spawned process
#   2. Working directory mismatch causing relative path failures
#   3. stdio pipe buffering issues between Node.js and the C++ binary
#   4. Missing environment variables that the binary depends on
#   5. Signal forwarding so the binary receives SIGTERM correctly
#
# Usage (in mcp.json or .trae/mcp.json):
#   {
#     "command": "bash",
#     "args": ["/absolute/path/to/mcpp_launcher.sh"]
#   }
# =============================================================================

# Do NOT use set -euo pipefail - it breaks LD_LIBRARY_PATH when unset
# Only exit on error
set -e

# ---------------------------------------------------------------------------
# Configuration - Adjust these paths to match your environment
# ---------------------------------------------------------------------------
MCPP_BINARY="${MCPP_BINARY:-/home/cat/Documents/trae_projects/trae/mcpp/build/mcpp_example}"
MCPP_PROJECT_DIR="${MCPP_PROJECT_DIR:-/home/cat/Documents/trae_projects/trae/mcpp}"

# ---------------------------------------------------------------------------
# Validate binary exists
# ---------------------------------------------------------------------------
if [ ! -x "$MCPP_BINARY" ]; then
    echo "[mcpp_launcher] ERROR: Binary not found or not executable: $MCPP_BINARY" >&2
    echo "[mcpp_launcher] Please set MCPP_BINARY environment variable or edit this script." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Environment Setup - CRITICAL: Build LD_LIBRARY_PATH carefully
# ---------------------------------------------------------------------------

# Build LD_LIBRARY_PATH components
LIB_PATHS="/usr/lib/x86_64-linux-gnu"

# Add project build directory
if [ -d "$MCPP_PROJECT_DIR/build" ]; then
    LIB_PATHS="$LIB_PATHS:$MCPP_PROJECT_DIR/build"
fi

# Add /usr/local/lib for custom installed libraries
LIB_PATHS="$LIB_PATHS:/usr/local/lib"

# Add existing LD_LIBRARY_PATH if set
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
    LIB_PATHS="$LIB_PATHS:$LD_LIBRARY_PATH"
fi

# Add conda lib if in conda environment
if [ -n "${CONDA_PREFIX:-}" ] && [ -d "$CONDA_PREFIX/lib" ]; then
    LIB_PATHS="$LIB_PATHS:$CONDA_PREFIX/lib"
fi

export LD_LIBRARY_PATH="$LIB_PATHS"

# Set working directory to the project directory
cd "$MCPP_PROJECT_DIR"

# Ensure PATH includes common binary locations
export PATH="/usr/local/bin:/usr/bin:/bin${PATH:+:$PATH}"

# ---------------------------------------------------------------------------
# Launch the MCP Server using exec to replace this shell process
# ---------------------------------------------------------------------------
# This ensures:
#   - No extra shell layer between Node.js spawn and the binary
#   - Signals (SIGTERM, SIGINT) reach the binary directly
#   - stdio pipes are directly connected without an intermediate process
exec "$MCPP_BINARY" "$@"
