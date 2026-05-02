#!/bin/bash
# =============================================================================
# search++ MCP Server Installer
# =============================================================================
# Automated installation script for search++ v1.0.0
# Supports: Linux x86_64
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/oemoem12/search-plus-plus-v1-mcp/main/install.sh | bash
#   OR
#   ./install.sh
# =============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Config
REPO="oemoem12/search-plus-plus-v1-mcp"
VERSION="v1.0.0"
INSTALL_DIR="${HOME}/.local/share/search-plus-plus-mcp"
BIN_NAME="mcpp_example"

# =============================================================================
# Functions
# =============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking dependencies..."

    # Check curl
    if ! command -v curl &> /dev/null; then
        log_error "curl is required but not installed."
        echo "Install with: sudo apt install curl (Ubuntu/Debian)"
        exit 1
    fi

    # Check libcurl
    if ! ldconfig -p | grep -q libcurl; then
        log_warn "libcurl not found in system paths."
        echo "Install with: sudo apt install libcurl4 (Ubuntu/Debian)"
        exit 1
    fi

    log_info "Dependencies OK"
}

download_release() {
    log_info "Downloading search++ v${VERSION}..."

    # Create temp directory
    TEMP_DIR=$(mktemp -d)
    trap "rm -rf ${TEMP_DIR}" EXIT

    # Download release tarball
    DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${VERSION}/search-plus-plus-v1-mcp-linux-x64.tar.gz"
    curl -sSL "${DOWNLOAD_URL}" -o "${TEMP_DIR}/release.tar.gz"

    # Extract
    log_info "Installing to ${INSTALL_DIR}..."
    mkdir -p "${INSTALL_DIR}"
    tar -xzf "${TEMP_DIR}/release.tar.gz" -C "${INSTALL_DIR}"

    # Rename directory
    if [ -d "${INSTALL_DIR}/linux-x64" ]; then
        mv "${INSTALL_DIR}/linux-x64" "${INSTALL_DIR}/search-plus-plus-v1-mcp"
        INSTALL_DIR="${INSTALL_DIR}/search-plus-plus-v1-mcp"
    fi

    # Make executable
    chmod +x "${INSTALL_DIR}/mcpp_example"
    chmod +x "${INSTALL_DIR}/mcpp_launcher.sh"

    log_info "Installation complete!"
}

setup_mcp_config() {
    log_info "Setting up MCP configuration..."

    # Detect Claude Desktop or Trae
    CLAUDE_CONFIG="${HOME}/Library/Application Support/Claude/claude_desktop_config.json"
    TRAE_CONFIG="${HOME}/.trae/mcp.json"

    if [ -f "${CLAUDE_CONFIG}" ]; then
        log_info "Found Claude Desktop config"
        # Add to Claude Desktop
        setup_claude_desktop
    elif [ -f "${TRAE_CONFIG}" ] || [ -d "${HOME}/.trae" ]; then
        log_info "Found Trae config"
        setup_trae
    else
        log_warn "No Claude Desktop or Trae config found."
        echo "You can manually configure the MCP server using:"
        echo "  ${INSTALL_DIR}/mcpp_example"
    fi
}

setup_claude_desktop() {
    CONFIG_FILE="${HOME}/Library/Application Support/Claude/claude_desktop_config.json"

    if [ ! -f "${CONFIG_FILE}" ]; then
        echo '{}' > "${CONFIG_FILE}"
    fi

    # Check if already configured
    if grep -q "search-plus-plus-v1-mcp" "${CONFIG_FILE}" 2>/dev/null; then
        log_info "Already configured for Claude Desktop"
        return
    fi

    # Add MCP server config using Python or jq
    if command -v python3 &> /dev/null; then
        python3 << EOF
import json
with open("${CONFIG_FILE}", "r") as f:
    config = json.load(f)

if "mcpServers" not in config:
    config["mcpServers"] = {}

config["mcpServers"]["search-plus-plus-v1-mcp"] = {
    "command": "${INSTALL_DIR}/mcpp_example"
}

with open("${CONFIG_FILE}", "w") as f:
    json.dump(config, f, indent=2)
print("OK")
EOF
    fi

    log_info "Claude Desktop configured!"
    log_warn "Please restart Claude Desktop to use the MCP server"
}

setup_trae() {
    CONFIG_FILE="${HOME}/.trae/mcp.json"

    mkdir -p "${HOME}/.trae"

    if [ ! -f "${CONFIG_FILE}" ]; then
        echo '{"mcpServers": {}}' > "${CONFIG_FILE}"
    fi

    # Check if already configured
    if grep -q "search-plus-plus-v1-mcp" "${CONFIG_FILE}" 2>/dev/null; then
        log_info "Already configured for Trae"
        return
    fi

    # Add MCP server config
    if command -v python3 &> /dev/null; then
        python3 << EOF
import json
with open("${CONFIG_FILE}", "r") as f:
    config = json.load(f)

if "mcpServers" not in config:
    config["mcpServers"] = {}

config["mcpServers"]["search-plus-plus-v1-mcp"] = {
    "command": "bash",
    "args": ["${INSTALL_DIR}/mcpp_launcher.sh"],
    "env": {
        "MCPP_BINARY": "${INSTALL_DIR}/mcpp_example",
        "MCPP_PROJECT_DIR": "${INSTALL_DIR}"
    }
}

with open("${CONFIG_FILE}", "w") as f:
    json.dump(config, f, indent=2)
print("OK")
EOF
    fi

    log_info "Trae configured!"
    log_warn "Please restart Trae to use the MCP server"
}

test_installation() {
    log_info "Testing installation..."

    TEST_OUTPUT=$("${INSTALL_DIR}/mcpp_example" <<EOF
{}
EOF
    )

    if echo "${TEST_OUTPUT}" | grep -q "jsonrpc"; then
        log_info "Test passed!"
    else
        log_warn "Test output unexpected, but binary may still work"
    fi
}

# =============================================================================
# Main
# =============================================================================

main() {
    echo "========================================"
    echo "  search++ MCP Server Installer"
    echo "  Version: ${VERSION}"
    echo "========================================"
    echo

    check_dependencies
    download_release
    test_installation

    echo
    log_info "Installation directory: ${INSTALL_DIR}"
    log_info "Binary: ${INSTALL_DIR}/mcpp_example"
    echo

    # Ask about MCP config
    if [ -z "${CI}" ]; then
        read -p "Setup MCP configuration? [Y/n]: " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            setup_mcp_config
        fi
    fi

    echo
    echo "========================================"
    echo "  Installation Complete!"
    echo "========================================"
}

main "$@"
