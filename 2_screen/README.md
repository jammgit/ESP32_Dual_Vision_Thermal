# ESP32 Dual Vision - Screen Test

屏幕测试项目，用于测试和验证 TFT 显示屏的各项功能。

## 硬件要求

- ESP32 开发板（支持 PSRAM）
- TFT 显示屏（支持 SPI 接口）
- USB 数据线

## 软件依赖

- PlatformIO 6.0+
- ESP32 Platform: espressif32@6.6.0
- Framework: Arduino
- 构建标志:
  - `BOARD_HAS_PSRAM`
  - `CONFIG_SPIRAM=1`

## 库依赖

- `TFT_eSPI@^2.5.43` - TFT 显示屏驱动库
- `TJpg_Decoder@^1.1.0` - JPEG 解码库
- `FS` - 文件系统支持

## 配置

- Flash 大小: 2MB
- 文件系统: LittleFS
- 分区配置: max_app_2MB.csv
- 串口波特率: 115200

## 快速开始

### 环境准备

1. 安装 PlatformIO Core
2. 克隆或下载本项目
3. 进入项目目录: `cd screen_test`

### 编译和上传

```bash
# 编译项目
pio run

# 上传到 ESP32
pio run --target upload

# 打开串口监视器
pio device monitor
```

### 完整流程

```bash
# 一次性编译、上传并监视
pio run --target upload && pio device monitor
```

## 功能特性

本项目用于测试 TFT 显示屏的以下功能:

- 基本图形绘制（点、线、矩形、圆形）
- 文字显示
- 颜色测试
- 触摸功能（如屏幕支持）
- JPEG 图像显示
- 刷新率测试

## 显示屏配置

TFT_eSPI 库需要在编译前进行配置。配置文件由 `assets/setup_tft_espi.py` 脚本处理。

## 目录结构

```
screen_test/
├── src/
│   └── main.cpp        # 主程序文件
├── .pio/               # PlatformIO 构建目录
├── max_app_2MB.csv     # 分区配置文件
├── platformio.ini      # 项目配置文件
└── README.md           # 本文档
```

## 常见 TFT 屏幕

以下是一些常用的 TFT 屏幕型号:

- ILI9341 (240x320)
- ST7735 (128x160)
- ST7789 (240x240, 240x320)
- ILI9486 (320x480)

## 故障排除

### 屏幕无显示

1. 检查 SPI 引脚连接
2. 确认屏幕供电（通常为 3.3V 或 5V）
3. 验证屏幕型号配置是否正确
4. 检查接线是否松动

### 颜色异常

1. 检查 RGB vs BGR 配置
2. 确认屏幕初始化参数

### 刷屏缓慢

1. 启用 PSRAM（确保开发板支持）
2. 检查 SPI 时钟频率设置
3. 使用 DMA 加速（如支持）

## 许可证

本项目遵循项目根目录的许可证。
