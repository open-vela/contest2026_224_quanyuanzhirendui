# ATK-DNN647 (STM32N6) Board Porting

openvela (NuttX) 适配正点原子 ATK-DNN647 开发板，基于 STM32N647X0 (Cortex-M55, 800MHz)。

## 适配进度

| 模块 | 状态 | 说明 |
|------|------|------|
| FSBL 启动加载器 | ✅ 完成 | VDD 电压域配置、SMPS 电源管理、XSPI Flash 初始化、DTR 模式切换、跳转应用 |
| XSPI Flash 驱动 | ✅ 完成 | MX25UM25645G (32MB)，1-1-1 SPI 初始化 → 8-8-8 DTR 模式 → Memory-Mapped XIP |
| 链接脚本 | ✅ 完成 | fsbl.ld (SRAM2)、flash_xip.ld (XIP)、flash.ld (DEV 调试) |
| NSH 终端 | ✅ 完成 | USART1 (PE5/PE6)，SRAM 和 XIP 两种运行模式 |
| LED 驱动 | ✅ 完成 | PG10 (LED0)、PE10 (LED1)，支持 auto/user 两种模式 |
| 烧录工具 | ✅ 完成 | flash_dnn647.sh 一键构建、签名、烧录 |
| GPIO 驱动 | ✅ 完成 | 基于 NuttX STM32 GPIO 框架 |
| 时钟配置 | ✅ 完成 | HSE 48MHz → PLL1 → CPU 200MHz / HCLK 50MHz |
| UART 驱动 | ✅ 完成 | USART1 串口控制台 |
| DMA | ⬜ 待做 | — |
| SPI/I2C 外设 | ⬜ 待做 | — |
| NPU (Cortex-M55) | ⬜ 待做 | — |
| MIPI CSI 显示 | ⬜ 待做 | — |

## 快速开始

```bash
# 构建并烧录 FSBL
./tools/flash_dnn647.sh fsbl

# 构建并烧录 NuttX 应用 (XIP)
./tools/flash_dnn647.sh app

# 全部构建烧录
./tools/flash_dnn647.sh all
```

串口连接 PE5(TX)/PE6(RX)，115200 baud，复位后进入 NSH 终端。
