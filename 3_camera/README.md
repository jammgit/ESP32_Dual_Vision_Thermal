# ESP32 Dual Vision - Camera Demo

相机演示项目，用于测试和演示 ESP32 摄像头模块的功能。

## 硬件要求

- ESP32 开发板（支持 PSRAM）
- 相机模块（如 OV2640、OV3660 等）
- TFT 显示屏
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
3. 进入项目目录: `cd camera_demo`

### 编译和上传

## 功能特性

- 相机图像捕获
- 图像在 TFT 屏幕显示
- JPEG 图像解码支持
- PSRAM 内存管理

## 目录结构

```
camera_demo/
├── src/
│   └── main.cpp        # 主程序文件
├── .pio/               # PlatformIO 构建目录
├── max_app_2MB.csv     # 分区配置文件
├── platformio.ini      # 项目配置文件
└── README.md           # 本文档
```

## 故障排除

### 上传失败
- 检查 USB 驱动是否正确安装
- 确认 COM 端口号
- 尝试按住 BOOT 按钮后按 RESET 按钮

### 显示异常
- 检查 TFT 引脚配置
- 确认电源供应充足

### 内存不足
- 确保使用支持 PSRAM 的 ESP32 开发板
- 检查 build_flags 中 PSRAM 配置

## 许可证

本项目遵循项目根目录的许可证。
