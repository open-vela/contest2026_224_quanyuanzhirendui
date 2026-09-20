#!/bin/bash
# check-env.sh - ATK-DNN647 开发环境检查
#

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "  ${GREEN}✓${NC} $1"; }
fail() { echo -e "  ${RED}✗${NC} $1"; }
warn() { echo -e "  ${YELLOW}!${NC} $1"; }

echo "=== ATK-DNN647 开发环境检查 ==="
echo ""

# ARM Toolchain
echo "[1] ARM 工具链"
if command -v arm-none-eabi-gcc &>/dev/null; then
    ver=$(arm-none-eabi-gcc --version | head -1)
    pass "arm-none-eabi-gcc: $ver"
else
    PATH_CHECK="/home/warner/openvela/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin"
    if [ -x "$PATH_CHECK/arm-none-eabi-gcc" ]; then
        ver=$("$PATH_CHECK/arm-none-eabi-gcc" --version | head -1)
        warn "arm-none-eabi-gcc found but not in PATH: $ver"
        echo "       Fix: export PATH=\"$PATH_CHECK:\$PATH\""
    else
        fail "arm-none-eabi-gcc not found"
    fi
fi

# STM32 Tools
echo ""
echo "[2] STM32 烧录工具"
STM32_DIR="/home/warner/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin"
if [ -x "$STM32_DIR/STM32_Programmer_CLI" ]; then
    pass "STM32_Programmer_CLI found"
else
    warn "STM32_Programmer_CLI not found at $STM32_DIR"
    echo "       烧录功能将不可用，可手动烧录"
fi
if [ -x "$STM32_DIR/STM32_SigningTool_CLI" ]; then
    pass "STM32_SigningTool_CLI found"
else
    warn "STM32_SigningTool_CLI not found"
    echo "       FSBL 签名将不可用"
fi

# NuttX source
echo ""
echo "[3] NuttX 源码"
NUTTX_DIR="/home/warner/openvela/nuttx"
if [ -f "$NUTTX_DIR/Makefile" ]; then
    pass "NuttX source found at $NUTTX_DIR"
else
    fail "NuttX source not found at $NUTTX_DIR"
fi

# Board configs
echo ""
echo "[4] 板级配置"
BOARD_DIR="$NUTTX_DIR/boards/arm/stm32n6/atk-dnn647"
for cfg in fsbl nsh nsh_xip leds; do
    if [ -f "$BOARD_DIR/configs/$cfg/defconfig" ]; then
        pass "Config '$cfg' found"
    else
        fail "Config '$cfg' missing"
    fi
done

# SWD connection
echo ""
echo "[5] SWD 调试器"
if [ -x "$STM32_DIR/STM32_Programmer_CLI" ]; then
    echo "       检测中..."
    if "$STM32_DIR/STM32_Programmer_CLI" -c port=SWD -l 2>/dev/null | grep -q "ST-Link"; then
        pass "ST-Link debugger detected"
    else
        warn "No ST-Link detected (connect board via SWD)"
    fi
else
    warn "Cannot check (STM32_Programmer_CLI not found)"
fi

echo ""
echo "=== 检查完成 ==="
