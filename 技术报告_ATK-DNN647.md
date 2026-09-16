# 2026 首届 openvela AI 硬件开发者大赛 · 技术报告

---

## 一、信息表

| 项目 | 内容 |
|------|------|
| **作品名称** | openvela on ATK-DNN647 (STM32N6) — 新硬件平台适配 |
| **队伍名称** | 全源智人队 (quanyuanzhirendui) |
| **团队分工** | 全栈开发（硬件适配、BSP 移植、FSBL 实现、工具链搭建） |
| **选题方向** | 新硬件平台适配 |

---

## 二、摘要

本作品将 openvela（基于 Apache NuttX 的 AIoT 操作系统）成功移植到正点原子 ATK-DNN647 开发板，搭载 STM32N647X0 芯片（Cortex-M55, 800MHz）。该芯片无内部 Flash，采用外部 XSPI NOR Flash（MX25UM25645G, 32MB）方案，启动架构与传统 STM32 差异显著。

核心成果包括：(1) 完整实现了 FSBL（First Stage Boot Loader），完成 VDD 电压域配置、SMPS 电源管理、XSPI Flash DTR 模式切换及 Memory-Mapped XIP 使能；(2) 设计了三套构建配置（FSBL / NSH-SRAM / NSH-XIP），覆盖从底层启动到应用运行的完整链路；(3) 提供一键构建烧录脚本，实现 FSBL 签名、编译、烧录全流程自动化。最终实现 NuttX 应用从外部 Flash XIP 执行，NSH 终端正常交互，LED 状态指示系统启动进度。

---

## 三、正文

### 3.1 绪论

#### 项目背景与问题定义

STM32N6 是意法半导体 2025 年推出的高性能 MCU 系列，基于 ARM Cortex-M55 内核，主频可达 800MHz，集成 NPU（Neural Processing Unit），面向边缘 AI、工业视觉、智能穿戴等场景。与传统 STM32 系列不同，STM32N6 **没有内部 Flash**，代码必须存储在外部 SPI NOR Flash 中，通过 XSPI 接口以 XIP（Execute-In-Place）方式执行。

这一架构变化带来了全新的启动挑战：
- **启动链复杂**：需要自研 FSBL 完成 Flash 初始化和模式切换
- **电源管理特殊**：多电压域（VDDIO2/3/4）需在启动早期配置
- **Flash 接口差异大**：XSPI 控制器支持 DTR（Double Transfer Rate）模式，时序配置复杂

openvela 作为小米开源的 AIoT 操作系统，目前主要支持 ARM Cortex-M 系列的传统 MCU。将 openvela 移植到 STM32N6 平台，为后续在该平台上运行 AI 应用（如 YOLO 目标检测）提供基础 OS 支撑。

#### 技术难点

1. **FSBL 实现**：STM32N6 的 Boot ROM 仅加载前 512KB 到 SRAM2，FSBL 必须在极小的 SRAM 空间内完成所有硬件初始化
2. **XSPI DTR 模式切换**：MX25UM25645G Flash 默认运行在 SPI 模式，需通过寄存器写入切换到 DOPI（8-8-8 DTR）模式
3. **Memory-Mapped 配置**：XSPI 控制器的 Memory-Mapped 模式需要精确的时序参数（dummy cycles、DQS 使能等）
4. **链接脚本适配**：三种运行模式（FSBL-SRAM2 / XIP-Flash / DEV-SRAM）需要不同的内存布局

#### 创新点

- **完整的 FSBL 实现**：参考 MicroPython 移植和 STM32N6 参考手册，从零实现了 VDD → SMPS → XSPI → XIP 的完整启动链
- **XIP 运行模式**：NuttX 应用直接从 32MB 外部 Flash 执行，节省宝贵的 SRAM 资源
- **LED 状态指示**：FSBL 启动过程中通过 LED 闪烁模式指示各阶段进度，便于调试
- **一键工具链**：封装了 FSBL 签名、构建、烧录的完整脚本，降低使用门槛

---

### 3.2 系统方案设计

#### 系统总体架构

```
┌─────────────────────────────────────────────────────────┐
│                    STM32N647X0 SoC                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ Cortex-M55│  │   NPU    │  │    XSPI Controller   │  │
│  │  800MHz   │  │          │  │    (XSPI2, DTR)      │  │
│  └─────┬────┘  └──────────┘  └──────────┬───────────┘  │
│        │                                │               │
│  ┌─────┴────────────────┐    ┌──────────┴───────────┐  │
│  │      SRAM2 (512KB)    │    │  MX25UM25645G (32MB) │  │
│  │  0x34180000-0x341FFFFF│    │  XSPI NOR Flash      │  │
│  │  ┌──────────────────┐ │    │  ┌──────────────────┐│  │
│  │  │ FSBL @ 0x34180400│ │    │  │ FSBL @ 0x70000000││  │
│  │  └──────────────────┘ │    │  │ App  @ 0x70080000││  │
│  └──────────────────────┘    │  └──────────────────┘│  │
│                               └─────────────────────┘  │
│  ┌──────────────────────┐                               │
│  │   SRAM1 (4MB)         │    ┌─────────────────────┐  │
│  │   0x34000400          │    │  USART1 (Console)    │  │
│  │   App Data/Stack      │    │  PE5=TX, PE6=RX      │  │
│  └──────────────────────┘    └─────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

**启动流程**：
```
Boot ROM (芯片内置)
    ↓ 从 Flash 0x70000000 加载 512KB 到 SRAM2
FSBL (0x34180400, SRAM2)
    ↓ 配置 VDDIO2/3/4 电压域
    ↓ 配置 SMPS 电源、电压缩放
    ↓ 初始化 XSPI2 控制器 (GPIO PN0-PN12, AF9)
    ↓ 切换 Flash 到 DOPI (8-8-8 DTR) 模式
    ↓ 使能 Memory-Mapped XIP (8DTRD 命令)
    ↓ 跳转到应用
NuttX App (0x70080000, XIP from Flash)
    ↓ 初始化 USART1 控制台
    ↓ NSH 终端就绪
```

#### 方案论证与选型

**开发板选型**：正点原子 ATK-DNN647 是目前市面上为数不多的 STM32N6 开发板，配备：
- STM32N647X0（Cortex-M55, 800MHz, NPU）
- MX25UM25645G（32MB XSPI NOR Flash, DTR 支持）
- 板载 ST-Link 调试器
- 丰富的外设接口（USART、SPI、I2C、MIPI 等）

**与备选方案对比**：

| 方案 | 优点 | 缺点 |
|------|------|------|
| SRAM 直接运行 | 简单，无需 FSBL | 仅 4MB SRAM，无法运行复杂应用 |
| XIP Flash 运行 | 32MB 空间，资源充足 | 需要 FSBL 初始化 Flash |
| RAM 加载执行 | 速度快 | SRAM 空间不足，不现实 |

**最终选择 XIP 方案**：通过 FSBL 初始化 Flash 后，应用代码直接从 Flash 执行，数据段加载到 SRAM，兼顾空间和性能。

#### 关键模块设计

1. **FSBL 模块**：最小化 NuttX 配置，禁用所有 OS 功能（无文件系统、无调度、无中断），仅保留裸机启动能力
2. **XSPI 驱动模块**：实现 1-1-1 SPI 模式写入（用于 Flash 配置寄存器操作）和 8-8-8 DTR Memory-Mapped 模式（用于 XIP 执行）
3. **链接脚本模块**：三套链接脚本分别适配 FSBL（SRAM2 @ 0x34180400）、XIP（Flash @ 0x70080000）、DEV（SRAM @ 0x34000400）

---

### 3.3 核心算法与技术原理

#### XSPI Flash DTR 模式切换算法

MX25UM25645G Flash 默认运行在 SPI（1-1-1）模式。要启用 DTR（8-8-8）模式，需要通过 Configuration Register 2 写入操作切换：

```
1. 发送 WREN (0x06) 命令 — 使能写操作
2. 轮询 RDSR (0x05) 等待 WEL 位 = 1
3. 写入 Configuration Register 2 (0x72) @ 地址 0x00000000，值 0x02 (DOPI)
4. Flash 重启进入 DOPI 模式
```

切换后，XSPI 控制器需要重新配置为 8-8-8 DTR 模式：
- 指令：8 线、DTR、16-bit（CMD_8DTRD = 0xEE11）
- 地址：8 线、DTR、32-bit
- 数据：8 线、DTR、DQS 使能
- Dummy Cycles：20（根据 Flash datasheet）

#### FSBL 跳转机制

FSBL 跳转到应用前需要：
1. 验证应用入口有效性（MSP 在 SRAM 范围、Reset Handler 在 Flash 范围）
2. 设置 NVIC 向量表指向应用地址
3. 清除 MSPLIM/PSPLIM（ARMv8.1-M 特有）
4. 设置 MSP 并执行 BX 跳转

```c
putreg32(app_addr, NVIC_VECTAB);
__asm__ volatile (
    "mov r0, #0\n"
    "msr msplim, r0\n"
    "msr psplim, r0\n"
    "msr msp, %0\n"
    "bx %1\n"
    : : "r" (msp), "r" (reset_handler) : "r0"
);
```

#### openvela 系统能力运用

本作品属于**新硬件适配**赛道，核心工作在 BSP 层面：
- **NuttX 内核**：使用 openvela 的 NuttX 内核作为 OS 基础
- **构建系统**：使用 openvela 的 `build.sh`、`tools/configure.sh` 等构建工具
- **板级框架**：遵循 NuttX 的 board 框架（Kconfig、Make.defs、链接脚本）
- **NSH 终端**：使用 NuttX 内置的 NSH Shell 作为交互界面

---

### 3.4 系统实现

#### 软件/固件架构

```
board/atk-dnn647/
├── Kconfig                    # FSBL / XIP 配置开关
├── configs/
│   ├── fsbl/defconfig         # FSBL 最小配置
│   ├── nsh/defconfig          # NSH SRAM 配置（调试）
│   └── nsh_xip/defconfig      # NSH XIP 配置（正式）
├── scripts/
│   ├── Make.defs              # 链接脚本选择逻辑
│   ├── fsbl.ld                # FSBL 链接脚本
│   ├── flash_xip.ld           # XIP 链接脚本
│   └── flash.ld               # DEV 模式链接脚本
├── include/board.h            # 时钟、LED、GPIO 定义
├── src/
│   ├── fsbl_main.c            # FSBL 主程序（833 行）
│   ├── stm32_boot.c           # NuttX 启动入口
│   ├── stm32_bringup.c        # 板级初始化
│   └── dnn647.h               # 板级私有头文件
└── tools/
    └── flash_dnn647.sh        # 一键构建烧录脚本
```

#### 数据流与关键流程

**FSBL 启动流程图**：

```
fsbl_main()
    │
    ├── up_irq_disable()          // 关中断
    ├── enable PWR clock          // 使能电源时钟
    ├── enable VDDIO4             // 使能 GPIO G 供电
    ├── led_init()                // 初始化状态 LED
    ├── LED x2 (FSBL 启动指示)
    │
    ├── fsbl_config_vdd()         // 配置 VDDIO2/3 电压域
    ├── LED x2 (VDD 完成)
    │
    ├── fsbl_config_power()       // 配置 SMPS、电压缩放
    ├── LED x2 (电源完成)
    │
    ├── xspi_init()               // XSPI 初始化
    │   ├── xspi_pin_config()     // PN0-PN12 配置为 AF9
    │   ├── enable XSPI clocks    // 使能 XSPI2/XSPIM 时钟
    │   ├── configure XSPI2       // Flash 类型、大小、时序
    │   ├── xspi_switch_to_dtr()  // 切换 Flash 到 DOPI 模式
    │   └── xspi_memory_map_888() // 使能 8DTRD Memory-Mapped
    ├── LED x2 (XSPI 完成)
    │
    └── fsbl_jump_to_app()        // 跳转到 0x70080000
        ├── 验证 MSP/Reset Handler
        ├── 设置 NVIC 向量表
        └── BX 跳转
```

#### 硬件设计与适配

| 项目 | 详情 |
|------|------|
| **芯片** | STM32N647X0 (Cortex-M55, 800MHz, NPU) |
| **开发板** | 正点原子 ATK-DNN647 |
| **外部 Flash** | MX25UM25645G (32MB, XSPI NOR, DTR 支持) |
| **XSPI 引脚** | PN0-PN12 (AF9), 包含 CLK/CS/DQ0-DQ7/DQS |
| **串口** | USART1: PE5(TX)/PE6(RX), 115200 baud |
| **LED** | PG10 (LED0, active low), PE10 (LED1, active low) |

**适配难点与解决方案**：

1. **无内部 Flash 的启动方案**：STM32N6 的 Boot ROM 从外部 Flash 加载 FSBL 到 SRAM2，FSBL 必须在 512KB 限制内完成所有初始化
   - *解决*：使用最小化 NuttX 配置，禁用所有 OS 功能，FSBL 仅 31KB

2. **XSPI DTR 模式切换时序**：Flash 需要先在 SPI 模式下写入配置寄存器，切换后 XSPI 控制器需重新配置
   - *解决*：实现 1-1-1 SPI 写入函数和 8-8-8 DTR Memory-Mapped 函数，分阶段初始化

3. **链接脚本多模式支持**：三种运行模式需要不同的内存布局
   - *解决*：通过 Kconfig 条件编译选择链接脚本，Make.defs/CMakeLists.txt 双构建系统支持

---

### 3.5 系统测试与结果分析

#### 测试环境

| 项目 | 配置 |
|------|------|
| 硬件 | ATK-DNN647 开发板 + ST-Link V3 |
| 工具链 | arm-none-eabi-gcc (GNU Arm Embedded Toolchain 13.3) |
| 烧录工具 | STM32CubeProgrammer 2.18 |
| 串口工具 | minicom / PuTTY, 115200 baud |
| 宿主机 | Ubuntu 22.04 (WSL2) |

#### 功能测试

| 测试项 | 预期结果 | 实际结果 | 状态 |
|--------|----------|----------|------|
| FSBL 编译 | 生成 nuttx.bin (< 31KB) | 生成 nuttx.bin (约 12KB) | ✅ |
| FSBL 签名 | 生成 nuttx_signed.bin | STM32_SigningTool 成功签名 | ✅ |
| FSBL 烧录 | 烧录到 0x70000000 | STM32_Programmer_CLI 成功烧录 | ✅ |
| FSBL LED 指示 | 启动过程中 LED 按序闪烁 | 4 组闪烁，指示各阶段完成 | ✅ |
| XIP 应用编译 | 生成 nuttx.bin (XIP 配置) | 编译成功 | ✅ |
| XIP 应用烧录 | 烧录到 0x70080000 | 烧录成功 | ✅ |
| NSH 终端 | 串口输出 NSH 提示符 | NSH> 正常显示，命令可执行 | ✅ |
| LED 用户控制 | nsh> leds 命令控制 LED | LED 正常开关 | ✅ |

#### 性能测试

| 指标 | 数据 |
|------|------|
| FSBL 启动时间 | 约 3 秒（含 LED 指示延迟） |
| FSBL 二进制大小 | ~12KB |
| NSH 启动时间 | < 1 秒（XIP 模式） |
| SRAM 占用 | 数据段 ~64KB，堆栈 ~4KB |
| Flash 占用 | 应用代码 ~256KB（XIP） |
| XSPI 时钟 | HCLK/4 = 50MHz / 4 = 12.5MHz |

---

### 3.6 AI-Native 开发说明

| 指标 | 数据 |
|------|------|
| **AI Coding 代码占比** | 约 60%（FSBL 核心逻辑、链接脚本、Kconfig、README） |
| **使用的 AI 工具** | Claude Code (Anthropic) |
| **MCP 工具使用情况** | 无 |
| **Skills 使用与新增情况** | 无 |
| **Token 使用总量** | 约 500K tokens |

**AI 工具对开发效率的提升**：

1. **寄存器级编程**：AI 辅助查阅 STM32N6 参考手册，快速定位 XSPI/PWR/RCC 寄存器地址和位定义
2. **DTR 模式切换**：AI 协助分析 MX25UM25645G datasheet 中的 DOPI 模式切换时序
3. **链接脚本编写**：AI 根据内存布局需求生成三套链接脚本
4. **调试排错**：通过 AI 分析 XSPI 初始化失败的原因（CSHT 配置、dummy cycles 数量等）
5. **文档生成**：README、工具说明文档由 AI 协助编写

**遇到的问题与解决方式**：
- AI 对 STM32N6 的寄存器地址偶有不准确，需对照参考手册验证
- XSPI DTR 模式的 dummy cycles 数量需要根据实际 Flash 型号调整

---

### 3.7 总结与展望

#### 成果总结

本作品成功将 openvela 移植到 STM32N6 (ATK-DNN647) 平台，实现了：
1. 完整的 FSBL 启动链（VDD → SMPS → XSPI → XIP → App）
2. 三种构建配置覆盖开发全流程
3. 一键构建烧录工具
4. NSH 终端正常交互

#### 应用前景与商业价值

- **目标受众**：嵌入式开发者、AI 边缘计算研究者、STM32N6 早期用户
- **应用场景**：工业视觉、智能穿戴、边缘 AI 推理（利用 Cortex-M55 + NPU）
- **商业潜力**：STM32N6 是 ST 未来重点推广的高性能 MCU，openvela 的适配为小米生态在该平台上的 AI 应用奠定基础

#### 不足与未来工作

- 当前 FSBL 启动较慢（LED 延迟占主要时间），可优化为仅保留关键指示
- 尚未验证 NPU 功能，后续需移植 AI 推理框架
- 可进一步适配更多外设（MIPI 摄像头、以太网、USB）

---

## 四、评审维度对照

| 评审维度（分值） | 报告对应章节 |
|------------------|--------------|
| 技术难度（30） | 3.2 系统方案设计；3.3 核心算法与技术原理；3.4 系统实现 |
| 产品创新性（20） | 二、摘要；3.1 绪论（创新点） |
| 项目完整度（20） | 3.5 系统测试与结果分析；项目源码 |
| AI 开发（10） | 3.6 AI-Native 开发说明 |
| 商业潜力（10） | 3.7 总结与展望 |
| 展示效果（10） | 演示视频；作品展示照片 |
