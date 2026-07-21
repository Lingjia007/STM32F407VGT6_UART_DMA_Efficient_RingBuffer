# STM32F407VGT6 UART DMA 高效收发环形缓冲区零拷贝

基于 [lwrb](https://github.com/MaJerle/lwrb) 轻量级环形缓冲区库，实现 UART DMA 循环接收 + 零拷贝发送的完整架构，运行在立创·天空星 STM32F407VGT6 开发板上。

> **配套博客文章**：[UART DMA 高效收发：环形缓冲区零拷贝实战，STM32 与 GD32 双芯片验证](https://lingjia007.github.io/Hugo_Web/p/uart-dma-%E9%AB%98%E6%95%88%E6%94%B6%E5%8F%91%E7%8E%AF%E5%BD%A2%E7%BC%93%E5%86%B2%E5%8C%BA%E9%9B%B6%E6%8B%B7%E8%B4%9D%E5%AE%9E%E6%88%98stm32-%E4%B8%8E-gd32-%E5%8F%8C%E8%8A%AF%E7%89%87%E9%AA%8C%E8%AF%81/)
>
> **GD32 双芯片验证工程**：[GD32F470ZGT6 UART DMA](https://github.com/Lingjia007/GD32F470ZGT6_UART_DMA_Efficient_RingBuffer)

## 核心架构

```
RX 通路：
[外部数据] → USART1_DR → DMA(Circular) → usart_rx_dma_buffer[64]
    ↓
    IDLE / HT / TC 三重中断触发
    ↓
    usart_rx_check() (DMA 计数器反推写入位置)
    ↓
    usart_process_data()
    ↓
    lwrb_write() → TX 环形缓冲区

TX 通路：
应用层 lwrb_write() → TX 环形缓冲区
    ↓
    usart_start_tx_dma_transfer() (零拷贝: DMA 直接从环形缓冲区线性块读取)
    ↓
    DMA(Normal) → USART1_DR → [外部]
    ↓
    TX 完成中断 → lwrb_skip() + 链式启动下一段
```

| 通路 | DMA 模式 | 说明 |
|------|----------|------|
| RX   | **Circular（循环）** | 接收是被动行为，循环模式让 DMA 自动回绕，永不停止 |
| TX   | **Normal（单次）** | 发送是主动行为，零拷贝从环形缓冲区取线性段，链式发送 |

## 硬件配置

| 项目 | 配置 |
|------|------|
| MCU | STM32F407VGT6 (立创·天空星) |
| UART | USART1, 115200bps, 8N1 |
| TX Pin | PA9 (GPIO_AF7_USART1) |
| RX Pin | PA10 (GPIO_AF7_USART1) |
| RX DMA | DMA2_Stream2_CH4, Circular |
| TX DMA | DMA2_Stream7_CH4, Normal |
| 调试接口 | J-Link |

## 关键设计

### RX 三重中断

| 中断 | 触发时机 | 作用 |
|------|----------|------|
| **HT (Half Transfer)** | DMA 写到缓冲区一半时 | 及时处理前半段数据，防止被覆盖 |
| **TC (Transfer Complete)** | DMA 写完整个缓冲区时 | 及时处理后半段数据，防止被覆盖 |
| **IDLE Line** | UART 检测到总线空闲时 | 处理不定长数据尾部 |

STM32 HAL 通过 `HAL_UARTEx_ReceiveToIdle_DMA()` 将三种中断统一到 `HAL_UARTEx_RxEventCallback()` 回调中。

### TX 零拷贝

DMA 直接从 lwrb 环形缓冲区的连续内存区域读取数据，无需中间 memcpy。跨回绕边界的数据自动拆分为多次 DMA 链式传输。

### DMA 计数器反推写入位置

```c
size_t pos = ARRAY_LEN(usart_rx_dma_buffer) - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
```

### NVIC 中断优先级

USART1、RX DMA、TX DMA 全部设为 `(0, 0)` 同优先级，防止 `usart_rx_check()` 被重入。

## 项目结构

```
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── usart.h
│   │   ├── dma.h
│   │   ├── gpio.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   └── stm32f4xx_it.h
│   └── Src/
│       ├── main.c            # 主程序：初始化、RX 检查、TX 零拷贝发送、中断回调
│       ├── usart.c           # UART 初始化、DMA 配置、printf 重定向
│       ├── dma.c             # DMA 时钟使能、NVIC 配置
│       ├── gpio.c            # GPIO 初始化
│       ├── stm32f4xx_it.c    # 中断服务函数入口
│       ├── stm32f4xx_hal_msp.c
│       └── system_stm32f4xx.c
├── Middleware/
│   └── lwrb/                 # 轻量级环形缓冲区库 (v2.0.0, by Tilen MAJERLE)
│       └── src/
│           ├── include/lwrb/lwrb.h
│           └── lwrb/lwrb.c
├── Drivers/
│   ├── CMSIS/                # Cortex-M4 核心支持
│   └── STM32F4xx_HAL_Driver/ # STM32F4 HAL 库
├── MDK-ARM/                  # Keil 工程文件及编译输出
├── STM32F407VGT6_UART_DMA_Efficient_RingBuffer.ioc  # CubeMX 配置
└── .mxproject
```

## 核心 API

| 函数 | 文件 | 说明 |
|------|------|------|
| `usart_rx_check()` | main.c | DMA 计数器反推写入位置，处理线性/回绕两种情况 |
| `usart_process_data()` | main.c | 将 RX 数据写入 TX 环形缓冲区（回环示例） |
| `usart_start_tx_dma_transfer()` | main.c | 零拷贝启动 TX DMA 传输，含临界区保护 |
| `usart_send_string()` | main.c | 字符串发送接口 |
| `HAL_UARTEx_RxEventCallback()` | main.c | RX 统一回调（HT/TC/IDLE） |
| `HAL_UART_TxCpltCallback()` | main.c | TX 完成回调：跳过已发数据 + 链式启动 |

## 缓冲区配置

| 缓冲区 | 大小 | 说明 |
|--------|------|------|
| `usart_rx_dma_buffer` | 64 字节 | RX DMA 循环缓冲区 |
| `usart_tx_rb_data` | 128 字节 | TX 环形缓冲区（lwrb） |

## 开发环境

- **IDE**：Keil MDK-ARM (μVision)
- **芯片支持**：STM32F407VGTx
- **配置工具**：STM32CubeMX
- **调试器**：J-Link
- **HAL 库版本**：STM32F4xx HAL Driver

## 使用方法

1. 使用 Keil MDK-ARM 打开 `MDK-ARM/STM32F407VGT6_UART_DMA_Efficient_RingBuffer.uvprojx`
2. 编译并下载到 STM32F407VGT6 开发板
3. 通过串口助手连接 USART1 (115200, 8N1)
4. 发送任意数据，开发板将回环发送（echo）

## 参考资料

- [lwrb - Lightweight ring buffer](https://github.com/MaJerle/lwrb)
- [stm32-usart-uart-dma-rx-tx](https://github.com/MaJerle/stm32-usart-uart-dma-rx-tx)
- [STM32F4xx HAL Driver](https://www.st.com/en/development-tools/stm32cubemx.html)

## License

本项目 MCU Drivers 目录下的 CMSIS 和 HAL 库遵循 STMicroelectronics 的 LICENSE。Middleware/lwrb 遵循 MIT License。用户代码部分可自由使用。
