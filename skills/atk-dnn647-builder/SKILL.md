---
name: atk-dnn647-builder
description: "ATK-DNN647 (STM32N6) 开发板构建、烧录、调试。Use when: 编译 FSBL、烧录 DNN647、XIP 模式、STM32N6 开发、atk-dnn647 board、boot ROM、XSPI flash。"
---

# ATK-DNN647 Builder

构建、烧录、调试 ATK-DNN647 (STM32N647X0) 开发板的 openvela 固件。

## 前置条件

- ARM 工具链: `prebuilts/gcc/linux-x86_64/arm-none-eabi/bin`
- STM32CubeProgrammer: `/home/warner/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin`
- 开发板通过 SWD 连接

## 构建配置

| 配置 | 路径 | 用途 |
|------|------|------|
| FSBL | `nuttx/boards/arm/stm32n6/atk-dnn647/configs/fsbl/` | 第一阶段启动加载器 |
| NSH (SRAM) | `nuttx/boards/arm/stm32n6/atk-dnn647/configs/nsh/` | 调试用，SRAM 运行 |
| NSH-XIP | `nuttx/boards/arm/stm32n6/atk-dnn647/configs/nsh_xip/` | 正式使用，Flash XIP |
| LED 测试 | `nuttx/boards/arm/stm32n6/atk-dnn647/configs/leds/` | LED 验证 |

## 一键构建烧录

```bash
cd nuttx

# 构建并烧录全部 (FSBL + App)
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh all

# 仅 FSBL
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh fsbl

# 仅应用 (XIP)
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh app

# 仅签名
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh sign
```

## 手动构建

```bash
cd nuttx

# 清理
make distclean

# 配置 (选择一个)
./tools/configure.sh -l boards/arm/stm32n6/atk-dnn647/configs/fsbl
./tools/configure.sh -l boards/arm/stm32n6/atk-dnn647/configs/nsh_xip
./tools/configure.sh -l boards/arm/stm32n6/atk-dnn647/configs/nsh

# 编译
make -j$(nproc)
```

## Flash 布局

```
0x70000000 ┌─────────────────┐
           │ FSBL (512KB)    │ 含 boot header
0x70080000 ├─────────────────┤
           │ NuttX App       │ XIP 执行
           │ (31.5MB)        │
0x72000000 └─────────────────┘
```

## 启动流程

```
Boot ROM → 加载 FSBL 到 SRAM2 (0x34180400)
         → FSBL 初始化 VDD/SMPS
         → FSBL 配置 XSPI Flash (DTR 模式)
         → FSBL 启用 Memory-Mapped
         → 跳转到 0x70080000 (XIP)
         → NuttX 启动
```

## 关键文件

| 文件 | 说明 |
|------|------|
| `src/fsbl_main.c` | FSBL 主程序 (VDD/XSPI/跳转) |
| `scripts/fsbl.ld` | FSBL 链接脚本 (SRAM2 @ 0x34180400) |
| `scripts/flash_xip.ld` | XIP 链接脚本 (Flash @ 0x70080000) |
| `include/board.h` | 时钟配置 (200MHz CPU)、GPIO 定义 |
| `Kconfig` | FSBL/XIP 配置开关 |

## 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| FSBL 不启动 | 签名头版本错误 | 检查 `-hv 2.3` 参数 |
| XIP 崩溃 | Flash 未初始化 | 先烧录并运行 FSBL |
| 串口无输出 | GPIO 配置错误 | 检查 PE5(TX)/PE6(RX) AF7 |
| 编译报错 CONFIG 缺失 | defconfig 不完整 | 使用 configs/ 下的 defconfig |
| arm-none-eabi-gcc 未找到 | PATH 未设置 | `export PATH=prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH` |

## 串口连接

- TX: PE5 (AF7)
- RX: PE6 (AF7)
- 波特率: 115200

## STM32N6 特殊说明

- **无内部 Flash**: 代码必须在外部 XSPI NOR Flash 或 SRAM 中运行
- **Boot ROM**: 芯片内置，从外部 Flash 加载前 512KB 到 SRAM2
- **FSBL 签名**: 必须使用 STM32_SigningTool 添加 boot header
- **DTR 模式**: MX25UM25645G Flash 支持 DTR，需在 FSBL 中切换
- **VDD 电压域**: 多个 I/O 电压域 (VDDIO2=3.3V, VDDIO3=1.8V, VDDIO4=3.3V)
