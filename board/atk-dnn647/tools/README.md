# ATK-DNN647 FSBL 工具说明

## 概述

本目录包含用于构建和烧录 ATK-DNN647 开发板 FSBL (First Stage Boot Loader) 的工具。

## 文件说明

- `flash_dnn647.sh` - 构建和烧录脚本
- `README.md` - 本说明文件

## 前置条件

1. 安装 ARM 工具链：
   ```bash
   # 工具链路径: /home/warner/openvela/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin
   ```

2. 安装 STM32 编程工具（可选）：
   - STM32_Programmer_CLI
   - STM32_SigningTool_cli

## 使用方法

### 构建 FSBL

```bash
cd /home/warner/openvela/nuttx
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh fsbl
```

### 构建 NuttX 应用程序 (XIP 模式)

```bash
cd /home/warner/openvela/nuttx
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh app
```

### 构建并烧录所有组件

```bash
cd /home/warner/openvela/nuttx
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh all
```

## Flash 布局

| 组件 | 地址 | 大小 | 说明 |
|------|------|------|------|
| FSBL | 0x70000000 | 512KB | First Stage Boot Loader |
| NuttX App | 0x70080000 | 31.5MB | NuttX 应用程序 (XIP) |

## 启动流程

1. **Boot ROM** → 从外部 Flash 加载前 512KB 到 SRAM2
2. **FSBL** (0x34180400) → 初始化 VDD、XSPI Flash
3. **NuttX App** (0x70080000) → 从 Flash XIP 执行

## 手动构建

如果需要手动构建：

```bash
cd /home/warner/openvela/nuttx

# 清理
make distclean

# 配置 FSBL
./tools/configure.sh -l boards/arm/stm32n6/atk-dnn647/configs/fsbl

# 构建
make -j$(nproc)

# 输出文件: nuttx.bin
```

## 手动烧录

使用 STM32_Programmer_CLI：

```bash
# 烧录 FSBL (需要先签名)
STM32_SigningTool_cli -s -bin nuttx.bin -o nuttx_signed.bin -t fsbl -hv 2.2
STM32_Programmer_CLI -c port=SWD -w nuttx_signed.bin 0x70000000 -v

# 烧录 NuttX 应用程序
STM32_Programmer_CLI -c port=SWD -w nuttx.bin 0x70080000 -v
```

## 故障排除

### 1. arm-none-eabi-gcc 未找到

确保工具链路径正确：
```bash
export PATH="/home/warner/openvela/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH"
```

### 2. FSBL 太大

当前 FSBL 限制为 31KB (31744 字节)。如果编译后太大，请：
- 检查 defconfig 中的配置
- 移除不必要的功能

### 3. 应用程序无法启动

检查：
- FSBL 是否正确烧录到 0x70000000
- NuttX 应用是否配置为 XIP 模式
- 应用链接地址是否为 0x70080000
