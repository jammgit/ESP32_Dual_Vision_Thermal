# ESP32 Dual Vision - Thermal Demo

热成像演示项目，用于测试和演示 ESP32 与热成像模块的集成功能。

## 硬件要求

- ESP32 开发板（支持 PSRAM）
- 热成像模块（如 MLX90640、AMG8833 等）
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
3. 进入项目目录: `cd thermal_demo`

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

本项目提供以下热成像功能:

- 热成像数据采集
- 实时温度数据显示
- 热力图渲染
- 温度标定功能
- 温度阈值报警
- 图像保存和回放

## 支持的热成像模块

### MLX90640

- 分辨率: 32x24 像素
- 视场角: 55° x 35°
- 温度范围: -40°C 到 300°C
- 接口: I2C

### AMG8833

- 分辨率: 8x8 像素
- 视场角: 60°
- 温度范围: 0°C 到 80°C
- 接口: I2C

## 应用场景

- 设备故障检测
- 人体温度监测
- 建筑热漏检测
- 夜视监控
- 科学实验

## 目录结构

```
thermal_demo/
├── src/
│   └── main.cpp        # 主程序文件
├── .pio/               # PlatformIO 构建目录
├── max_app_2MB.csv     # 分区配置文件
├── platformio.ini      # 项目配置文件
└── README.md           # 本文档
```

## 故障排除

### 无法读取热成像数据

1. 检查 I2C 接线（SDA、SCL）
2. 确认 I2C 地址正确
3. 验证模块供电（通常为 3.3V）
4. 使用 I2C 扫描工具检查连接

### 显示温度异常

1. 检查环境温度补偿设置
2. 验证发射率参数配置
3. 确认温度单位设置（摄氏度/华氏度）
4. 执行传感器校准

### 更新缓慢

1. 检查 I2C 时钟频率
2. 减少数据显示的刷新间隔
3. 优化渲染算法

## 许可证

本项目遵循项目根目录的许可证。
