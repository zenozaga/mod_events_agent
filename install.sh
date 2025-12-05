#!/bin/bash
# install.sh - Installation script for mod_event_agent inside FreeSWITCH container
# Run this script INSIDE the FreeSWITCH Docker container

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

info() {
    echo -e "${BLUE}ℹ${NC}  $1"
}

success() {
    echo -e "${GREEN}✓${NC}  $1"
}

error() {
    echo -e "${RED}✗${NC}  $1"
}

warning() {
    echo -e "${YELLOW}⚠${NC}  $1"
}

# Detect if running inside container
if [ ! -f "/.dockerenv" ]; then
    error "This script must be run INSIDE the FreeSWITCH Docker container"
    info "Use: docker exec -it agent_nats_dev_freeswitch bash"
    info "Then: cd /workspace && ./install.sh"
    exit 1
fi

echo "╔════════════════════════════════════════════════════════╗"
echo "║  mod_event_agent - Container Installation              ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Check prerequisites
info "Step 1: Installing build prerequisites..."

apt-get update -qq
apt-get install -y build-essential libcjson-dev libssl-dev > /dev/null 2>&1

success "Prerequisites installed"
echo ""

# Step 2: Verify workspace
info "Step 2: Verifying workspace..."

WORKSPACE_DIR="/workspace"

if [ ! -f "$WORKSPACE_DIR/Makefile" ]; then
    error "Makefile not found in $WORKSPACE_DIR"
    exit 1
fi

if [ ! -f "$WORKSPACE_DIR/lib/nats/libnats_static.a" ]; then
    error "NATS static library not found at lib/nats/libnats_static.a"
    exit 1
fi

if [ ! -f "$WORKSPACE_DIR/include/nats/nats.h" ]; then
    error "NATS headers not found at include/nats/nats.h"
    exit 1
fi

success "Workspace verified"
echo ""

# Step 3: Detect FreeSWITCH headers
info "Step 3: Detecting FreeSWITCH headers..."

# Check if FreeSWITCH headers are already installed in the system
if [ -d "/usr/local/freeswitch/include/freeswitch" ]; then
    FREESWITCH_INCLUDE_DIR="/usr/local/freeswitch/include/freeswitch"
    success "Using system FreeSWITCH headers: $FREESWITCH_INCLUDE_DIR"
else
    error "FreeSWITCH headers not found in /usr/local/freeswitch/include/freeswitch"
    info "This Docker image should have FreeSWITCH pre-installed with headers"
    exit 1
fi

echo ""

# Step 4: Clean previous builds
info "Step 4: Cleaning previous builds..."

cd "$WORKSPACE_DIR"
make clean > /dev/null 2>&1 || true

success "Cleaned"
echo ""

# Step 5: Compile the module
info "Step 5: Compiling mod_event_agent..."

if make WITH_NATS=1 FREESWITCH_INCLUDE_DIR="$FREESWITCH_INCLUDE_DIR"; then
    success "Module compiled successfully"
else
    error "Compilation failed"
    exit 1
fi

echo ""

# Step 6: Install the module
info "Step 6: Installing module..."

FREESWITCH_MOD_DIR="/usr/local/freeswitch/mod"

if [ ! -d "$FREESWITCH_MOD_DIR" ]; then
    error "FreeSWITCH module directory not found: $FREESWITCH_MOD_DIR"
    exit 1
fi

cp mod_event_agent.so "$FREESWITCH_MOD_DIR/"
chmod 755 "$FREESWITCH_MOD_DIR/mod_event_agent.so"

success "Module installed to: $FREESWITCH_MOD_DIR/mod_event_agent.so"
echo ""

# Step 7: Install NATS shared library
info "Step 7: Installing NATS shared library..."

if [ -f "$WORKSPACE_DIR/lib/nats/libnats.so.3.8.2" ]; then
    cp "$WORKSPACE_DIR/lib/nats/libnats.so"* /usr/local/lib/
    ldconfig
    success "NATS library installed to /usr/local/lib/"
else
    warning "NATS shared library not found (using static)"
fi

echo ""

# Step 8: Install configuration
info "Step 8: Installing configuration..."

FREESWITCH_CONF_DIR="/usr/local/freeswitch/conf/autoload_configs"

if [ -d "$FREESWITCH_CONF_DIR" ]; then
    cp autoload_configs/mod_event_agent.conf.xml "$FREESWITCH_CONF_DIR/"
    success "Configuration installed to: $FREESWITCH_CONF_DIR/mod_event_agent.conf.xml"
else
    warning "Configuration directory not found: $FREESWITCH_CONF_DIR"
fi

echo ""

# Step 9: Verify installation
info "Step 9: Verifying installation..."

if [ -f "$FREESWITCH_MOD_DIR/mod_event_agent.so" ]; then
    success "Module file exists"
    
    # Check dependencies
    if ldd "$FREESWITCH_MOD_DIR/mod_event_agent.so" | grep -q "not found"; then
        error "Missing dependencies:"
        ldd "$FREESWITCH_MOD_DIR/mod_event_agent.so" | grep "not found"
        exit 1
    else
        success "All dependencies resolved"
    fi
else
    error "Module file not found after installation"
    exit 1
fi

echo ""

# Summary
echo "╔════════════════════════════════════════════════════════╗"
echo "║  Installation Complete!                                 ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""
success "Module: $FREESWITCH_MOD_DIR/mod_event_agent.so"
[ -f "$FREESWITCH_CONF_DIR/mod_event_agent.conf.xml" ] && success "Config: $FREESWITCH_CONF_DIR/mod_event_agent.conf.xml"
echo ""
info "Next steps:"
echo "  1. Restart FreeSWITCH or reload the module:"
echo "     fs_cli -x 'reload mod_event_agent'"
echo ""
echo "  2. Verify module is loaded:"
echo "     fs_cli -x 'module_exists mod_event_agent'"
echo ""
echo "  3. Check module configuration:"
echo "     cat $FREESWITCH_CONF_DIR/mod_event_agent.conf.xml"
echo ""
info "NATS Server: Make sure NATS is running at nats://agent-dev-nats:4222"
echo ""
