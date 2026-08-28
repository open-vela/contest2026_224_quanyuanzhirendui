#!/bin/bash
# flash_dnn647.sh - Build and flash script for ATK-DNN647 board
#
# This script builds the FSBL and NuttX application, then flashes them
# to the external SPI flash on the DNN647 development board.
#
# Usage: ./flash_dnn647.sh [fsbl|app|all|sign]
#

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOARD_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
NUTTX_DIR="$(cd "$BOARD_DIR/../../../.." && pwd)"

# Toolchain path
export PATH="/home/warner/openvela/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH"

# STM32 tools path
STM32_TOOLS="/home/warner/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Flash addresses
FSBL_ADDR=0x70000000
APP_ADDR=0x70080000

# Header version for DNN647
HEADER_VERSION="2.3"

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Build FSBL
build_fsbl() {
    log_info "Building FSBL..."
    cd "$NUTTX_DIR"

    # Clean previous build
    make distclean 2>/dev/null || true

    # Configure for FSBL
    ./tools/configure.sh -l boards/arm/stm32n6/atk-dnn647/configs/fsbl

    # Build
    make -j$(nproc)

    if [ -f nuttx.bin ]; then
        log_info "FSBL built successfully: nuttx.bin ($(wc -c < nuttx.bin) bytes)"
    else
        log_error "FSBL build failed!"
        exit 1
    fi
}

# Sign FSBL binary
sign_fsbl() {
    log_info "Signing FSBL binary..."

    if [ ! -f "$NUTTX_DIR/nuttx.bin" ]; then
        log_error "FSBL binary not found. Build first."
        exit 1
    fi

    if [ ! -f "$STM32_TOOLS/STM32_SigningTool_CLI" ]; then
        log_error "STM32_SigningTool_CLI not found at: $STM32_TOOLS"
        exit 1
    fi

    # Remove old signed file if exists
    rm -f "$NUTTX_DIR/nuttx_signed.bin"

    # Sign the FSBL binary for boot ROM
    # -nk: no key (just add header, no cryptographic signature)
    # -of 0x80000000: output format
    # -t fsbl: type is FSBL
    # -hv 2.3: header version for DNN647
    # --align: align the binary
    "$STM32_TOOLS/STM32_SigningTool_CLI" \
        -bin "$NUTTX_DIR/nuttx.bin" \
        -nk \
        -of 0x80000000 \
        -t fsbl \
        -o "$NUTTX_DIR/nuttx_signed.bin" \
        -hv "$HEADER_VERSION" \
        --align

    if [ -f "$NUTTX_DIR/nuttx_signed.bin" ]; then
        log_info "FSBL signed successfully: nuttx_signed.bin ($(wc -c < "$NUTTX_DIR/nuttx_signed.bin") bytes)"
    else
        log_error "FSBL signing failed!"
        exit 1
    fi
}

# Build NuttX application (XIP mode)
build_app() {
    log_info "Building NuttX application (XIP mode)..."
    cd "$NUTTX_DIR"

    # Clean previous build
    make distclean 2>/dev/null || true

    # Configure for XIP NSH
    ./tools/configure.sh -l boards/arm/stm32n6/atk-dnn647/configs/nsh_xip

    # Build
    make -j$(nproc)

    if [ -f nuttx.bin ]; then
        log_info "Application built successfully: nuttx.bin ($(wc -c < nuttx.bin) bytes)"
    else
        log_error "Application build failed!"
        exit 1
    fi
}

# Flash FSBL using STM32_Programmer_CLI
flash_fsbl() {
    log_info "Flashing FSBL to $FSBL_ADDR..."

    if [ ! -f "$NUTTX_DIR/nuttx_signed.bin" ]; then
        log_error "Signed FSBL binary not found. Run 'sign' first."
        exit 1
    fi

    # Flash using STM32_Programmer_CLI
    if [ -f "$STM32_TOOLS/STM32_Programmer_CLI" ]; then
        "$STM32_TOOLS/STM32_Programmer_CLI" -c port=SWD \
            -w "$NUTTX_DIR/nuttx_signed.bin" "$FSBL_ADDR" \
            -v
    else
        log_error "STM32_Programmer_CLI not found!"
        log_info "Please flash manually: nuttx_signed.bin -> $FSBL_ADDR"
        exit 1
    fi
}

# Flash NuttX application
flash_app() {
    log_info "Flashing NuttX application to $APP_ADDR..."

    if [ ! -f "$NUTTX_DIR/nuttx.bin" ]; then
        log_error "Application binary not found. Build first."
        exit 1
    fi

    # Flash using STM32_Programmer_CLI
    if [ -f "$STM32_TOOLS/STM32_Programmer_CLI" ]; then
        "$STM32_TOOLS/STM32_Programmer_CLI" -c port=SWD \
            -w "$NUTTX_DIR/nuttx.bin" "$APP_ADDR" \
            -v
    else
        log_error "STM32_Programmer_CLI not found!"
        log_info "Please flash manually: nuttx.bin -> $APP_ADDR"
        exit 1
    fi
}

# Main
case "${1:-all}" in
    fsbl)
        build_fsbl
        sign_fsbl
        flash_fsbl
        ;;
    sign)
        sign_fsbl
        ;;
    app)
        build_app
        flash_app
        ;;
    all)
        build_fsbl
        sign_fsbl
        flash_fsbl
        build_app
        flash_app
        ;;
    *)
        echo "Usage: $0 [fsbl|sign|app|all]"
        echo ""
        echo "Commands:"
        echo "  fsbl  - Build, sign and flash FSBL"
        echo "  sign  - Sign FSBL binary only"
        echo "  app   - Build and flash NuttX application only"
        echo "  all   - Build, sign and flash both (default)"
        exit 1
        ;;
esac

log_info "Done!"
