# ATK-DNN647 (STM32N6) 新硬件适配

## 一、作品简介

将 openvela (NuttX) 实时操作系统移植到 **正点原子 ATK-DNN647** 开发板上，基于 STM32N647X0 芯片。该芯片无内部 Flash，采用外部 XSPI NOR Flash (MX25UM25645G, 32MB) 方案。本作品实现了完整的启动链：FSBL → XSPI Flash 初始化 → XIP 应用执行，并提供了一键构建烧录工具。

**亮点：**
- 完整的 FSBL (First Stage Boot Loader) 实现，包括 VDD 电压域配置、SMPS 电源管理、XSPI Flash DTR 模式切换
- XIP (Execute-In-Place) 运行模式，应用直接从外部 Flash 执行，节省 SRAM
- 三种构建配置：FSBL / NSH (SRAM) / NSH-XIP (Flash)
- 一键烧录脚本，支持 FSBL 签名、构建、烧录全流程

## 二、选题方向

**新硬件适配**

STM32N6 是 ST 最新推出的高性能 Cortex-M55 MCU，主频可达 800MHz，集成 NPU，但无内部 Flash，启动方案与传统 STM32 差异较大。将 openvela 移植到该平台，为后续 AI 应用（如 YOLO 推理）提供基础 OS 支撑。

## 三、目录结构

```
board/atk-dnn647/
├── Kconfig                    # 板级 Kconfig 配置（FSBL / XIP 开关）
├── CMakeLists.txt             # CMake 构建入口
├── configs/
│   ├── fsbl/defconfig         # FSBL 最小配置（无 OS 功能）
│   ├── nsh/defconfig          # NSH 终端配置（SRAM 运行，调试用）
│   ├── nsh_xip/defconfig      # NSH XIP 配置（Flash 运行，正式使用）
│   └── leds/defconfig         # LED 测试配置
├── scripts/
│   ├── Make.defs              # 链接脚本选择逻辑
│   ├── fsbl.ld                # FSBL 链接脚本（SRAM2 @ 0x34180400）
│   ├── flash_xip.ld           # XIP 链接脚本（Flash @ 0x70080000）
│   └── flash.ld               # DEV 模式链接脚本（SRAM @ 0x34000400）
├── include/
│   └── board.h                # 板级头文件（时钟、LED、GPIO 定义）
├── src/
│   ├── fsbl_main.c            # FSBL 主程序（VDD/XSPI/Flash 初始化 + 跳转）
│   ├── stm32_boot.c           # NuttX 启动入口（XIP 模式 LED 闪烁验证）
│   ├── stm32_bringup.c        # 板级后期初始化
│   ├── stm32_autoleds.c       # 自动 LED 控制（OS 事件指示）
│   ├── stm32_userleds.c       # 用户 LED 控制
│   ├── dnn647.h               # 板级私有头文件
│   ├── CMakeLists.txt         # 源文件 CMake 配置
│   └── Make.defs              # 源文件 Make 配置
└── tools/
    ├── flash_dnn647.sh        # 一键构建烧录脚本
    └── README.md              # 工具使用说明
```

## 四、运行方式

### 前置条件

- ARM 工具链：`arm-none-eabi-gcc`（openvela 自带 prebuilts）
- STM32CubeProgrammer（用于烧录和签名）
- ATK-DNN647 开发板 + SWD 调试器

### 步骤 1：拉取完整工程

```bash
repo init -u https://github.com/open-vela/contest2026_224_quanyuanzhirendui \
  -b dev-ai-contest-2026 -m contest2026_224_quanyuanzhirendui.xml
repo sync -c -j8
```

### 步骤 2：构建并烧录 FSBL

```bash
cd nuttx
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh fsbl
```

该命令会：
1. 配置 FSBL 最小配置
2. 编译生成 `nuttx.bin`
3. 使用 STM32_SigningTool 签名
4. 通过 SWD 烧录到 0x70000000

### 步骤 3：构建并烧录 NuttX 应用 (XIP)

```bash
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh app
```

### 步骤 4：运行

复位开发板，FSBL 会：
1. LED 闪烁指示启动进度
2. 初始化 XSPI Flash（切换到 DTR 模式）
3. 启用 Memory-Mapped 模式
4. 跳转到 0x70080000 执行 NuttX 应用

串口连接 PE5(TX)/PE6(RX)，115200 baud，可进入 NSH 终端。

### 一键全部构建烧录

```bash
./boards/arm/stm32n6/atk-dnn647/tools/flash_dnn647.sh all
```

## 五、Flash 布局

| 组件 | 地址 | 大小 | 说明 |
|------|------|------|------|
| FSBL | 0x70000000 | 512KB | 第一阶段启动加载器 |
| NuttX App | 0x70080000 | 31.5MB | NuttX 应用程序 (XIP) |

## 六、启动流程

```
Boot ROM (芯片内置)
    ↓ 加载 FSBL 到 SRAM2
FSBL (0x34180400)
    ↓ 配置 VDD 电压域
    ↓ 配置 SMPS 电源
    ↓ 初始化 XSPI Flash (DTR 模式)
    ↓ 启用 Memory-Mapped (XIP)
    ↓ 跳转到应用
NuttX App (0x70080000, XIP from Flash)
    ↓ NSH 终端就绪
```

## 七、AI Coding 使用说明

本作品在以下环节借助 AI 辅助开发：

- **方案设计**：与 AI 讨论 STM32N6 启动流程、XSPI Flash 配置方案、DTR 模式切换时序
- **编码实现**：FSBL 核心代码（fsbl_main.c）在 AI 辅助下完成寄存器级操作、XSPI 驱动编写
- **调试排错**：通过 AI 分析链接脚本配置、内存布局问题、Flash 命令时序
- **文档生成**：README、工具说明文档由 AI 协助编写

完整对话日志见 `logs/` 目录。
