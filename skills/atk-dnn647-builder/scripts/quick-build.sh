#!/bin/bash
# quick-build.sh - ATK-DNN647 快速构建脚本
#
# Usage: ./quick-build.sh [fsbl|nsh|nsh_xip|leds|all]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Navigate to nuttx root (relative to skill location in contest repo)
NUTTX_DIR="$(cd "$SCRIPT_DIR/../../../../nuttx" 2>/dev/null || cd "$SCRIPT_DIR/../../../nuttx" 2>/dev/null || echo "/home/warner/openvela/nuttx")"

export PATH="/home/warner/openvela/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

build_config() {
    local config=$1
    log_info "Building config: $config"
    cd "$NUTTX_DIR"
    make distclean 2>/dev/null || true
    ./tools/configure.sh -l boards/arm/stm32n6/atk-dnn647/configs/$config
    make -j$(nproc)

    if [ -f nuttx.bin ]; then
        local size=$(wc -c < nuttx.bin)
        log_info "Build successful: nuttx.bin ($size bytes)"
        if [ "$size" -gt 31744 ] && [ "$config" = "fsbl" ]; then
            log_warn "FSBL size ($size) exceeds 31KB limit! May not fit in SRAM2."
        fi
    else
        log_error "Build failed!"
        exit 1
    fi
}

show_help() {
    echo "ATK-DNN647 Quick Build Script"
    echo ""
    echo "Usage: $0 [config]"
    echo ""
    echo "Configs:"
    echo "  fsbl     - FSBL (First Stage Boot Loader)"
    echo "  nsh      - NSH terminal (SRAM, for debugging)"
    echo "  nsh_xip  - NSH XIP (Flash execution, production)"
    echo "  leds     - LED test"
    echo "  all      - Build all configs sequentially"
    echo ""
    echo "Output: nuttx/nuttx.bin"
}

case "${1:-help}" in
    fsbl|nsh|nsh_xip|leds)
        build_config "$1"
        ;;
    all)
        for cfg in fsbl nsh nsh_xip leds; do
            build_config "$cfg"
            echo ""
        done
        ;;
    *)
        show_help
        ;;
esac
